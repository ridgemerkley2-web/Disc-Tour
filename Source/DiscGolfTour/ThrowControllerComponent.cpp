#include "ThrowControllerComponent.h"
#include "DiscGolfMath.h"

UThrowControllerComponent::UThrowControllerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UThrowControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bTimingActive) return;

    TimingElapsed += DeltaTime;
    const float Cycle = FMath::Max(TimingCycleSeconds, 0.25f);
    TimingNeedle01 = FMath::Fmod(TimingElapsed / Cycle, 1.0f);
}

void UThrowControllerComponent::AdjustPower(float AxisValue, float DeltaSeconds)
{
    if (bTimingActive) return;
    const float Rate = ShotContext == EDiscShotContext::Drive ? 0.42f : 0.30f;
    const float Minimum = ShotContext == EDiscShotContext::Drive ? 0.35f : 0.20f;
    Power01 = FMath::Clamp(Power01 + AxisValue * DeltaSeconds * Rate, Minimum, 1.0f);
}

void UThrowControllerComponent::AdjustHyzer(float AxisValue, float DeltaSeconds)
{
    if (bTimingActive) return;
    const float Limit = ShotContext == EDiscShotContext::Drive ? 30.0f : 12.0f;
    const float Rate = ShotContext == EDiscShotContext::Drive ? 35.0f : 18.0f;
    HyzerDeg = FMath::Clamp(HyzerDeg + AxisValue * DeltaSeconds * Rate, -Limit, Limit);
}

void UThrowControllerComponent::AdjustNose(float AxisValue, float DeltaSeconds)
{
    if (bTimingActive) return;
    const float Minimum = ShotContext == EDiscShotContext::Drive ? -4.0f : -2.0f;
    const float Maximum = ShotContext == EDiscShotContext::Drive ? 8.0f : 5.0f;
    NoseDeg = FMath::Clamp(NoseDeg + AxisValue * DeltaSeconds * 12.0f, Minimum, Maximum);
}

void UThrowControllerComponent::ToggleThrowStyle()
{
    if (bTimingActive) return;
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
    LaunchAngleDeg = NewContext == EDiscShotContext::Circle1Putt ? 14.0f : 12.0f;
    TimingCycleSeconds = NewContext == EDiscShotContext::Circle1Putt ? 1.05f : 0.98f;
    IdealTiming01 = 0.76f;
    TimingMissSpan01 = NewContext == EDiscShotContext::Circle1Putt ? 0.145f : 0.125f;
}

bool UThrowControllerComponent::HandleThrowPress(FName MoldId, EDiscPlastic Plastic, const FVector& Direction, FThrowCommand& OutCommand)
{
    if (!bTimingActive)
    {
        bTimingActive = true;
        TimingElapsed = 0.0f;
        TimingNeedle01 = 0.0f;
        return false;
    }

    const float TimingError = DiscGolfMath::NormalizeTimingError(TimingNeedle01, IdealTiming01, TimingMissSpan01);

    OutCommand.MoldId = MoldId;
    OutCommand.Plastic = Plastic;
    OutCommand.ThrowStyle = ThrowStyle;
    OutCommand.ShotContext = ShotContext;
    OutCommand.Direction = Direction.GetSafeNormal();
    OutCommand.Power01 = Power01;
    OutCommand.HyzerDeg = HyzerDeg;
    OutCommand.NoseAngleDeg = NoseDeg;
    OutCommand.LaunchAngleDeg = LaunchAngleDeg;
    OutCommand.TimingError = TimingError;

    CancelTiming();
    return true;
}
