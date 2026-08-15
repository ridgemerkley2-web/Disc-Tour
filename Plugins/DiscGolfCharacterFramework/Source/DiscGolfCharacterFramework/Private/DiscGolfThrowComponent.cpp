#include "DiscGolfThrowComponent.h"
#include "DiscGolfCharacterProfile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

UDiscGolfThrowComponent::UDiscGolfThrowComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDiscGolfThrowComponent::SetThrowIntent(const FDGThrowIntent& NewIntent)
{
    CurrentIntent = NewIntent;
    CurrentIntent.Power01 = FMath::Clamp(CurrentIntent.Power01, 0.0f, 1.0f);
    CurrentIntent.TimingQuality01 = FMath::Clamp(CurrentIntent.TimingQuality01, 0.0f, 1.0f);
    CurrentIntent.HyzerDegrees = FMath::Clamp(CurrentIntent.HyzerDegrees, -60.0f, 60.0f);
    CurrentIntent.NoseDegrees = FMath::Clamp(CurrentIntent.NoseDegrees, -20.0f, 20.0f);
}

void UDiscGolfThrowComponent::BeginThrow()
{
    bThrowActive = true;
    bDiscReleased = false;
    CurrentPhase = EDGThrowPhase::Aim;
    LastGripSampleTimeSeconds = -1.0;
    OnThrowPhaseChanged.Broadcast(CurrentPhase);
}

void UDiscGolfThrowComponent::CancelThrow()
{
    bThrowActive = false;
    bDiscReleased = false;
    CurrentPhase = EDGThrowPhase::Idle;
    LastGripSampleTimeSeconds = -1.0;
    OnThrowPhaseChanged.Broadcast(CurrentPhase);
}

void UDiscGolfThrowComponent::NotifyThrowPhase(EDGThrowPhase NewPhase)
{
    CurrentPhase = NewPhase;
    OnThrowPhaseChanged.Broadcast(CurrentPhase);
}

FName UDiscGolfThrowComponent::GetActiveDiscGripBone() const
{
    if (!CharacterProfile)
    {
        return TEXT("disc_grip_r");
    }

    return CharacterProfile->Handedness == EDGHandedness::Right
        ? CharacterProfile->RightDiscGripBone
        : CharacterProfile->LeftDiscGripBone;
}

void UDiscGolfThrowComponent::NotifyDiscRelease(USkeletalMeshComponent* MeshComp)
{
    if (!bThrowActive || bDiscReleased || !MeshComp)
    {
        return;
    }

    const FName GripBone = GetActiveDiscGripBone();
    const FTransform GripTransform = MeshComp->GetSocketTransform(GripBone, RTS_World);

    FDGReleaseData Data;
    Data.GripWorldTransform = GripTransform;
    Data.Intent = CurrentIntent;
    Data.Handedness = CharacterProfile ? CharacterProfile->Handedness : EDGHandedness::Right;

    FDGThrowCapability Capability;
    FDGThrowStyle Style;

    if (CharacterProfile)
    {
        Capability = CharacterProfile->GetCapability(CurrentIntent.ThrowType);
        Style = CharacterProfile->ThrowStyle;
    }
    else
    {
        Capability.MinLaunchSpeedMps = 8.0f;
        Capability.MaxLaunchSpeedMps = 32.0f;
        Capability.MinSpinRpm = 250.0f;
        Capability.MaxSpinRpm = 1400.0f;
    }

    const float Power = FMath::Clamp(CurrentIntent.Power01, 0.0f, 1.0f);
    const float Timing = FMath::Clamp(CurrentIntent.TimingQuality01, 0.0f, 1.0f);
    const float TimingSpeedScale = FMath::Lerp(0.92f, 1.0f, Timing);
    const float TimingSpinScale = FMath::Lerp(0.90f, 1.0f, Timing);

    Data.SuggestedLaunchSpeedMps =
        FMath::Lerp(Capability.MinLaunchSpeedMps, Capability.MaxLaunchSpeedMps, Power)
        * Style.PowerMultiplier
        * TimingSpeedScale;

    Data.SuggestedSpinRpm =
        FMath::Lerp(Capability.MinSpinRpm, Capability.MaxSpinRpm, Power)
        * Style.SpinMultiplier
        * TimingSpinScale;

    if (UWorld* World = GetWorld())
    {
        const double Now = World->GetTimeSeconds();
        const FVector Location = GripTransform.GetLocation();

        if (LastGripSampleTimeSeconds >= 0.0 && Now > LastGripSampleTimeSeconds)
        {
            const double Dt = Now - LastGripSampleTimeSeconds;
            Data.GripLinearVelocityCmPerSec = (Location - LastGripWorldLocation) / Dt;
        }

        LastGripWorldLocation = Location;
        LastGripSampleTimeSeconds = Now;
    }

    bDiscReleased = true;
    CurrentPhase = EDGThrowPhase::Release;
    OnThrowPhaseChanged.Broadcast(CurrentPhase);
    OnDiscRelease.Broadcast(Data);
}

void UDiscGolfThrowComponent::NotifyThrowFinished()
{
    bThrowActive = false;
    CurrentPhase = EDGThrowPhase::Idle;
    LastGripSampleTimeSeconds = -1.0;
    OnThrowPhaseChanged.Broadcast(CurrentPhase);
    OnThrowFinished.Broadcast();
}
