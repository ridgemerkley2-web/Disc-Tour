#include "ThrowControllerComponent.h"
#include "DiscGolfMath.h"

UThrowControllerComponent::UThrowControllerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UThrowControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceTiming(DeltaTime);
}

void UThrowControllerComponent::AdvanceTiming(float DeltaTime)
{
    if (!bTimingActive) return;

    if (!FMath::IsFinite(DeltaTime)
        || DeltaTime < 0.0f
        || !FMath::IsFinite(TimingElapsed)
        || !FMath::IsFinite(TimingCycleSeconds)
        || TimingCycleSeconds <= 0.0f)
    {
        CancelTiming();
        return;
    }

    TimingElapsed += DeltaTime;
    if (!FMath::IsFinite(TimingElapsed))
    {
        CancelTiming();
        return;
    }
    const float Cycle = FMath::Max(TimingCycleSeconds, 0.25f);
    TimingNeedle01 = FMath::Fmod(TimingElapsed / Cycle, 1.0f);
    if (!FMath::IsFinite(TimingNeedle01))
    {
        CancelTiming();
    }
}

void UThrowControllerComponent::AdjustPower(float AxisValue, float DeltaSeconds)
{
    if (bTimingActive
        || !FMath::IsFinite(AxisValue)
        || !FMath::IsFinite(DeltaSeconds)
        || DeltaSeconds < 0.0f
        || !FMath::IsFinite(Power01)) return;
    const float Rate = ShotContext == EDiscShotContext::Drive ? 0.42f : 0.30f;
    const float Minimum = ShotContext == EDiscShotContext::Drive ? 0.35f : 0.20f;
    Power01 = FMath::Clamp(Power01 + AxisValue * DeltaSeconds * Rate, Minimum, 1.0f);
}

void UThrowControllerComponent::AdjustHyzer(float AxisValue, float DeltaSeconds)
{
    if (bTimingActive
        || !FMath::IsFinite(AxisValue)
        || !FMath::IsFinite(DeltaSeconds)
        || DeltaSeconds < 0.0f
        || !FMath::IsFinite(HyzerDeg)) return;
    const float Limit = ShotContext == EDiscShotContext::Drive ? 30.0f : 12.0f;
    const float Rate = ShotContext == EDiscShotContext::Drive ? 35.0f : 18.0f;
    HyzerDeg = FMath::Clamp(HyzerDeg + AxisValue * DeltaSeconds * Rate, -Limit, Limit);
}

void UThrowControllerComponent::AdjustNose(float AxisValue, float DeltaSeconds)
{
    if (bTimingActive
        || !FMath::IsFinite(AxisValue)
        || !FMath::IsFinite(DeltaSeconds)
        || DeltaSeconds < 0.0f
        || !FMath::IsFinite(NoseDeg)) return;
    const float Minimum = ShotContext == EDiscShotContext::Drive ? -4.0f : -2.0f;
    const float Maximum = ShotContext == EDiscShotContext::Drive ? 8.0f : 5.0f;
    NoseDeg = FMath::Clamp(NoseDeg + AxisValue * DeltaSeconds * 12.0f, Minimum, Maximum);
}

void UThrowControllerComponent::ToggleThrowStyle()
{
    if (bTimingActive || !DiscGolfMath::IsValidThrowStyle(ThrowStyle)) return;
    ThrowStyle = ThrowStyle == EThrowStyle::Backhand ? EThrowStyle::Forehand : EThrowStyle::Backhand;
}

void UThrowControllerComponent::CancelTiming()
{
    bTimingActive = false;
    TimingElapsed = 0.0f;
    TimingNeedle01 = 0.0f;
}

void UThrowControllerComponent::SetShotContext(EDiscShotContext NewContext, float DistanceToBasketMeters)
{
    if (!DiscGolfMath::IsValidDiscShotContext(NewContext)
        || !FMath::IsFinite(DistanceToBasketMeters))
    {
        return;
    }

    CancelTiming();
    const bool bContextChanged = ShotContext != NewContext;
    ShotContext = NewContext;
    PuttDistanceMeters = NewContext == EDiscShotContext::Drive ? 0.0f : FMath::Max(DistanceToBasketMeters, 0.0f);

    if (NewContext == EDiscShotContext::Drive)
    {
        RecommendedPower01 = 0.82f;
        if (bContextChanged)
        {
            Power01 = 0.82f;
            HyzerDeg = 3.0f;
            NoseDeg = 1.0f;
            LaunchAngleDeg = 7.0f;
        }
        TimingCycleSeconds = 1.30f;
        IdealTiming01 = 0.82f;
        TimingMissSpan01 = 0.18f;
        return;
    }

    RecommendedPower01 = DiscGolfMath::RecommendedPuttPower01(PuttDistanceMeters);
    Power01 = RecommendedPower01;
    HyzerDeg = NewContext == EDiscShotContext::Circle1Putt ? 0.0f : 1.0f;
    NoseDeg = 1.0f;
    LaunchAngleDeg = DiscGolfMath::RecommendedPuttLaunchAngleDeg(
        NewContext, PuttDistanceMeters);
    TimingCycleSeconds = NewContext == EDiscShotContext::Circle1Putt ? 1.05f : 0.98f;
    IdealTiming01 = 0.76f;
    TimingMissSpan01 = NewContext == EDiscShotContext::Circle1Putt ? 0.145f : 0.125f;
}

void UThrowControllerComponent::SetAccessibilityAssist(
    float NewAimAssist01,
    float NewTimingWindowScale)
{
    if (!FMath::IsFinite(NewAimAssist01)
        || !FMath::IsFinite(NewTimingWindowScale))
    {
        return;
    }

    AimAssist01 = FMath::Clamp(NewAimAssist01, 0.0f, 1.0f);
    TimingWindowScale = FMath::Clamp(NewTimingWindowScale, 0.5f, 2.0f);
}

FVector UThrowControllerComponent::ApplyAimAssistToDirection(
    const FVector& Direction,
    const FVector& TargetDirection,
    float Assist01)
{
    if (!FMath::IsFinite(Direction.X)
        || !FMath::IsFinite(Direction.Y)
        || !FMath::IsFinite(Direction.Z)
        || !FMath::IsFinite(TargetDirection.X)
        || !FMath::IsFinite(TargetDirection.Y)
        || !FMath::IsFinite(TargetDirection.Z)
        || !FMath::IsFinite(Assist01))
    {
        return FVector::ZeroVector;
    }

    const FVector SafeDirection = Direction.GetSafeNormal();
    const float SafeAssist = FMath::Clamp(Assist01, 0.0f, 1.0f);
    if (SafeAssist <= 0.0f || SafeDirection.IsNearlyZero())
    {
        // Preserve the accepted zero-assist command path exactly.
        return SafeDirection;
    }

    const FVector Direction2D = FVector(SafeDirection.X, SafeDirection.Y, 0.0f).GetSafeNormal();
    const FVector Target2D = FVector(TargetDirection.X, TargetDirection.Y, 0.0f).GetSafeNormal();
    if (Direction2D.IsNearlyZero() || Target2D.IsNearlyZero())
    {
        return SafeDirection;
    }

    constexpr float MaximumCorrectionDegrees = 6.0f;
    const float SignedErrorDegrees = FMath::RadiansToDegrees(FMath::Atan2(
        FVector::CrossProduct(Direction2D, Target2D).Z,
        FVector::DotProduct(Direction2D, Target2D)));
    const float CorrectionDegrees = FMath::Clamp(
        SignedErrorDegrees,
        -MaximumCorrectionDegrees * SafeAssist,
        MaximumCorrectionDegrees * SafeAssist);
    return SafeDirection.RotateAngleAxis(CorrectionDegrees, FVector::UpVector).GetSafeNormal();
}

float UThrowControllerComponent::NormalizeTimingErrorForAccessibility(
    float TimingNeedle01,
    float IdealTiming01,
    float TimingMissSpan01,
    float WindowScale)
{
    if (!FMath::IsFinite(TimingNeedle01)
        || !FMath::IsFinite(IdealTiming01)
        || !FMath::IsFinite(TimingMissSpan01)
        || !FMath::IsFinite(WindowScale))
    {
        return DiscGolfMath::ThrowCommandMaximumTimingError + 1.0f;
    }

    const float SafeScale = FMath::Clamp(WindowScale, 0.5f, 2.0f);
    return DiscGolfMath::NormalizeTimingError(
        TimingNeedle01,
        IdealTiming01,
        TimingMissSpan01 * SafeScale);
}

bool UThrowControllerComponent::HandleThrowPress(FName MoldId, EDiscPlastic Plastic, const FVector& Direction, FThrowCommand& OutCommand)
{
    return HandleThrowPress(MoldId, Plastic, Direction, Direction, OutCommand);
}

bool UThrowControllerComponent::HandleThrowPress(
    FName MoldId,
    EDiscPlastic Plastic,
    const FVector& Direction,
    const FVector& AimAssistTargetDirection,
    FThrowCommand& OutCommand)
{
    const float TimingError = bTimingActive
        ? NormalizeTimingErrorForAccessibility(
            TimingNeedle01, IdealTiming01, TimingMissSpan01, TimingWindowScale)
        : 0.0f;
    const FThrowCommand Candidate = BuildCommandCandidate(
        MoldId,
        Plastic,
        Direction,
        AimAssistTargetDirection,
        TimingError);

    if (!DiscGolfMath::IsThrowCommandValid(Candidate))
    {
        if (bTimingActive)
        {
            CancelTiming();
        }
        return false;
    }

    if (!bTimingActive)
    {
        bTimingActive = true;
        TimingElapsed = 0.0f;
        TimingNeedle01 = 0.0f;
        return false;
    }

    CancelTiming();
    OutCommand = Candidate;
    return true;
}

FThrowCommand UThrowControllerComponent::BuildCommandCandidate(
    FName MoldId,
    EDiscPlastic Plastic,
    const FVector& Direction,
    const FVector& AimAssistTargetDirection,
    float TimingError) const
{
    FThrowCommand Candidate;
    Candidate.MoldId = MoldId;
    Candidate.Plastic = Plastic;
    Candidate.ThrowStyle = ThrowStyle;
    Candidate.ShotContext = ShotContext;
    Candidate.Direction = ApplyAimAssistToDirection(
        Direction, AimAssistTargetDirection, AimAssist01);
    Candidate.Power01 = Power01;
    Candidate.HyzerDeg = HyzerDeg;
    Candidate.NoseAngleDeg = NoseDeg;
    Candidate.LaunchAngleDeg = LaunchAngleDeg;
    Candidate.TimingError = TimingError;
    return Candidate;
}
