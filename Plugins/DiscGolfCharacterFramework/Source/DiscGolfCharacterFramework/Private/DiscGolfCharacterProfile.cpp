#include "DiscGolfCharacterProfile.h"

UDiscGolfCharacterProfile::UDiscGolfCharacterProfile()
{
    FDGThrowCapability Backhand;
    Backhand.MinLaunchSpeedMps = 8.0f;
    Backhand.MaxLaunchSpeedMps = 32.0f;
    Backhand.MinSpinRpm = 250.0f;
    Backhand.MaxSpinRpm = 1400.0f;
    Capabilities.Add(EDGThrowType::Backhand, Backhand);

    FDGThrowCapability Forehand;
    Forehand.MinLaunchSpeedMps = 8.0f;
    Forehand.MaxLaunchSpeedMps = 30.0f;
    Forehand.MinSpinRpm = 200.0f;
    Forehand.MaxSpinRpm = 1200.0f;
    Capabilities.Add(EDGThrowType::Forehand, Forehand);

    FDGThrowCapability Standstill;
    Standstill.MinLaunchSpeedMps = 6.0f;
    Standstill.MaxLaunchSpeedMps = 24.0f;
    Standstill.MinSpinRpm = 180.0f;
    Standstill.MaxSpinRpm = 1050.0f;
    Capabilities.Add(EDGThrowType::Standstill, Standstill);

    FDGThrowCapability Putt;
    Putt.MinLaunchSpeedMps = 3.0f;
    Putt.MaxLaunchSpeedMps = 13.0f;
    Putt.MinSpinRpm = 100.0f;
    Putt.MaxSpinRpm = 650.0f;
    Capabilities.Add(EDGThrowType::Putt, Putt);
}

FDGThrowCapability UDiscGolfCharacterProfile::GetCapability(EDGThrowType ThrowType) const
{
    if (const FDGThrowCapability* Found = Capabilities.Find(ThrowType))
    {
        return *Found;
    }

    return FDGThrowCapability();
}
