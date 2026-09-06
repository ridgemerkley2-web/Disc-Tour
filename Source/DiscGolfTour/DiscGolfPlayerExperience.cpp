#include "DiscGolfPlayerExperience.h"

void FDiscGolfPlayerSettings::Normalize()
{
    SchemaVersion = 1;
    GraphicsQuality = FMath::Clamp(GraphicsQuality, 0, 3);
    ResolutionX = FMath::Clamp(ResolutionX, 1280, 7680);
    ResolutionY = FMath::Clamp(ResolutionY, 720, 4320);
    WindowMode = FMath::Clamp(WindowMode, 0, 2);
    MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
    MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
    EffectsVolume = FMath::Clamp(EffectsVolume, 0.0f, 1.0f);
    AmbienceVolume = FMath::Clamp(AmbienceVolume, 0.0f, 1.0f);
    VoiceVolume = FMath::Clamp(VoiceVolume, 0.0f, 1.0f);
    MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.25f, 3.0f);
    ControllerSensitivity = FMath::Clamp(ControllerSensitivity, 0.25f, 3.0f);
    ControllerDeadZone = FMath::Clamp(ControllerDeadZone, 0.0f, 0.95f);
    CameraShakeStrength = FMath::Clamp(CameraShakeStrength, 0.0f, 1.0f);
    ReplaySpeed = FMath::Clamp(ReplaySpeed, 0.25f, 2.0f);
    HudScale = FMath::Clamp(HudScale, 0.75f, 1.35f);
    TextScale = FMath::Clamp(TextScale, 0.85f, 1.30f);
    AimAssist01 = FMath::Clamp(AimAssist01, 0.0f, 1.0f);
    TimingWindowScale = FMath::Clamp(TimingWindowScale, 0.5f, 2.0f);
    ColorVisionMode = static_cast<EDiscGolfColorVisionMode>(FMath::Clamp(
        static_cast<int32>(ColorVisionMode),
        static_cast<int32>(EDiscGolfColorVisionMode::None),
        static_cast<int32>(EDiscGolfColorVisionMode::Tritanopia)));
    TracerColorPreset = static_cast<EDiscGolfTracerColorPreset>(FMath::Clamp(
        static_cast<int32>(TracerColorPreset),
        static_cast<int32>(EDiscGolfTracerColorPreset::SignalTeal),
        static_cast<int32>(EDiscGolfTracerColorPreset::PaperWhite)));
}

void UDiscGolfThrowHistorySubsystem::RecordThrow(const FDiscGolfThrowHistoryEntry& Entry)
{
    if (Entry.HoleNumber <= 0 || Entry.ShotNumber <= 0 || Entry.DiscId.IsNone()) return;
    while (Entries.Num() >= MaxHistoryEntries) Entries.RemoveAt(0, 1, EAllowShrinking::No);
    Entries.Add(Entry);
}

FString DiscGolfPlayerExperience::FormatDistance(
    float DistanceMeters,
    EDiscGolfUnitSystem Units,
    bool bIncludeToPin)
{
    const float SafeMeters = FMath::Max(0.0f, DistanceMeters);
    const FString Value = Units == EDiscGolfUnitSystem::Metric
        ? FString::Printf(TEXT("%.0f M"), SafeMeters)
        : FString::Printf(TEXT("%.0f FT"), SafeMeters * 3.280839895f);
    return bIncludeToPin ? Value + TEXT(" TO PIN") : Value;
}

FString DiscGolfPlayerExperience::FormatWind(float SpeedMps, EDiscGolfUnitSystem Units)
{
    const float SafeSpeed = FMath::Max(0.0f, SpeedMps);
    return Units == EDiscGolfUnitSystem::Metric
        ? FString::Printf(TEXT("%.1f M/S"), SafeSpeed)
        : FString::Printf(TEXT("%.0f MPH"), SafeSpeed * 2.236936292f);
}

FString DiscGolfPlayerExperience::HoleScoreName(int32 Strokes, int32 Par)
{
    if (Strokes <= 0 || Par <= 0) return TEXT("SCORE");
    if (Strokes == 1) return TEXT("ACE");
    const int32 Relative = Strokes - Par;
    switch (Relative)
    {
        case -4: return TEXT("CONDOR");
        case -3: return TEXT("ALBATROSS");
        case -2: return TEXT("EAGLE");
        case -1: return TEXT("BIRDIE");
        case 0: return TEXT("PAR");
        case 1: return TEXT("BOGEY");
        case 2: return TEXT("DOUBLE BOGEY");
        case 3: return TEXT("TRIPLE BOGEY");
        default: return Relative > 0
            ? FString::Printf(TEXT("+%d"), Relative)
            : FString::Printf(TEXT("%d"), Relative);
    }
}

FString DiscGolfPlayerExperience::DiscClassName(int32 Speed)
{
    if (Speed >= 10) return TEXT("DISTANCE DRIVER");
    if (Speed >= 7) return TEXT("FAIRWAY DRIVER");
    if (Speed >= 4) return TEXT("MIDRANGE");
    return TEXT("PUTTER");
}

FString DiscGolfPlayerExperience::LandingName(EDiscGolfLandingClassification Landing)
{
    switch (Landing)
    {
        case EDiscGolfLandingClassification::SemiRough: return TEXT("SEMI-ROUGH");
        case EDiscGolfLandingClassification::DeepRough: return TEXT("DEEP ROUGH");
        case EDiscGolfLandingClassification::Green: return TEXT("GREEN");
        case EDiscGolfLandingClassification::OutOfBounds: return TEXT("OUT OF BOUNDS");
        case EDiscGolfLandingClassification::Hazard: return TEXT("HAZARD");
        case EDiscGolfLandingClassification::Fairway:
        default: return TEXT("FAIRWAY");
    }
}

EDiscGolfLandingClassification DiscGolfPlayerExperience::ClassifyLanding(
    const FDiscGolfLieState& Lie,
    bool bHoledOut)
{
    if (bHoledOut || Lie.LieType == ELieType::Circle1 || Lie.LieType == ELieType::Circle2)
    {
        return EDiscGolfLandingClassification::Green;
    }
    if (Lie.PenaltyType == EDiscGolfPenaltyType::OutOfBounds)
    {
        return EDiscGolfLandingClassification::OutOfBounds;
    }
    if (Lie.PenaltyType == EDiscGolfPenaltyType::Hazard || Lie.LieType == ELieType::Hazard)
    {
        return EDiscGolfLandingClassification::Hazard;
    }
    if (Lie.LieType == ELieType::DeepRough || Lie.PlayingSurface == ECourseSurfaceType::DeepRough)
    {
        return EDiscGolfLandingClassification::DeepRough;
    }
    if (Lie.LieType == ELieType::LightRough || Lie.PlayingSurface == ECourseSurfaceType::LightRough)
    {
        return EDiscGolfLandingClassification::SemiRough;
    }
    return EDiscGolfLandingClassification::Fairway;
}

float DiscGolfPlayerExperience::BasketMarkerOpacity(
    const FDiscGolfPlayerSettings& Settings,
    bool bBasketDirectlyVisible,
    float DistanceMeters)
{
    if (!Settings.bHudVisible || !Settings.bBasketMarkerVisible) return 0.0f;
    if (!bBasketDirectlyVisible) return 0.92f;
    if (DistanceMeters <= 10.0f) return 0.0f;
    return DistanceMeters >= 25.0f ? 0.42f : 0.22f;
}

FLinearColor DiscGolfPlayerExperience::ResolveTracerColor(EDiscGolfTracerColorPreset Preset)
{
    switch (Preset)
    {
        case EDiscGolfTracerColorPreset::AccessibleLime:
            return FLinearColor(0.72f, 0.94f, 0.18f, 1.0f);
        case EDiscGolfTracerColorPreset::TournamentAmber:
            return FLinearColor(0.96f, 0.67f, 0.20f, 1.0f);
        case EDiscGolfTracerColorPreset::PaperWhite:
            return FLinearColor(0.96f, 0.95f, 0.89f, 1.0f);
        case EDiscGolfTracerColorPreset::SignalTeal:
        default:
            return FLinearColor(0.18f, 0.78f, 0.64f, 1.0f);
    }
}

FString DiscGolfPlayerExperience::ColorVisionModeName(EDiscGolfColorVisionMode Mode)
{
    switch (Mode)
    {
        case EDiscGolfColorVisionMode::Protanopia: return TEXT("PROTANOPIA");
        case EDiscGolfColorVisionMode::Deuteranopia: return TEXT("DEUTERANOPIA");
        case EDiscGolfColorVisionMode::Tritanopia: return TEXT("TRITANOPIA");
        case EDiscGolfColorVisionMode::None:
        default: return TEXT("NONE");
    }
}

FString DiscGolfPlayerExperience::TracerColorPresetName(EDiscGolfTracerColorPreset Preset)
{
    switch (Preset)
    {
        case EDiscGolfTracerColorPreset::AccessibleLime: return TEXT("ACCESSIBLE LIME");
        case EDiscGolfTracerColorPreset::TournamentAmber: return TEXT("TOURNAMENT AMBER");
        case EDiscGolfTracerColorPreset::PaperWhite: return TEXT("PAPER WHITE");
        case EDiscGolfTracerColorPreset::SignalTeal:
        default: return TEXT("SIGNAL TEAL");
    }
}

void DiscGolfPlayerExperience::BuildBoundedReplaySamples(
    const TArray<FDiscTrajectorySample>& Source,
    TArray<FDiscTrajectorySample>& OutSamples,
    int32 MaxSamples)
{
    OutSamples.Reset();
    MaxSamples = FMath::Max(MaxSamples, 2);
    if (Source.Num() <= MaxSamples)
    {
        OutSamples = Source;
        return;
    }

    TSet<int32> Required;
    Required.Add(0);
    Required.Add(Source.Num() - 1);
    for (int32 Index = 1; Index < Source.Num(); ++Index)
    {
        const FDiscTrajectorySample& Previous = Source[Index - 1];
        const FDiscTrajectorySample& Current = Source[Index];
        if (Previous.GroundState != Current.GroundState
            || Previous.GroundContactCount != Current.GroundContactCount)
        {
            Required.Add(Index - 1);
            Required.Add(Index);
        }
    }

    TArray<int32> Selected = Required.Array();
    Selected.Sort();
    const int32 RemainingBudget = FMath::Max(0, MaxSamples - Selected.Num());
    for (int32 Slot = 1; Slot <= RemainingBudget; ++Slot)
    {
        const int32 Index = FMath::RoundToInt(
            static_cast<double>(Slot) * static_cast<double>(Source.Num() - 1)
            / static_cast<double>(RemainingBudget + 1));
        Required.Add(FMath::Clamp(Index, 1, Source.Num() - 2));
    }
    Selected = Required.Array();
    Selected.Sort();
    if (Selected.Num() > MaxSamples)
    {
        // Pathological transition density is still bounded; endpoints remain mandatory.
        TArray<int32> Bounded;
        Bounded.Add(0);
        for (int32 Slot = 1; Slot < MaxSamples - 1; ++Slot)
        {
            const int32 Pick = FMath::RoundToInt(
                static_cast<double>(Slot) * static_cast<double>(Selected.Num() - 1)
                / static_cast<double>(MaxSamples - 1));
            Bounded.AddUnique(Selected[Pick]);
        }
        Bounded.Add(Source.Num() - 1);
        Selected = MoveTemp(Bounded);
        Selected.Sort();
    }
    OutSamples.Reserve(Selected.Num());
    for (const int32 Index : Selected) OutSamples.Add(Source[Index]);
}
