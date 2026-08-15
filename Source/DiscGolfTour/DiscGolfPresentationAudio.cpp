#include "DiscGolfPresentationAudio.h"

namespace
{
float SafeClamp(float Value, float Minimum, float Maximum, float Fallback = 0.0f)
{
    return FMath::Clamp(FMath::IsFinite(Value) ? Value : Fallback, Minimum, Maximum);
}

float SafeUnit(float Value, float Fallback = 0.0f)
{
    return SafeClamp(Value, 0.0f, 1.0f, Fallback);
}

float SafeRatio(float Value, float Maximum)
{
    return SafeClamp(Value, 0.0f, Maximum) / Maximum;
}

FDiscGolfPresentationAudioEvent MakeEvent(
    EDiscGolfPresentationAudioCategory Category,
    const TCHAR* EventId,
    const TCHAR* Label,
    float Intensity01,
    float PitchMultiplier)
{
    if (Category == EDiscGolfPresentationAudioCategory::Invalid
        || EventId == nullptr || EventId[0] == TEXT('\0')
        || Label == nullptr || Label[0] == TEXT('\0'))
    {
        return {};
    }

    FDiscGolfPresentationAudioEvent Event;
    Event.ContractVersion = DiscGolfPresentationAudio::CurrentContractVersion;
    Event.Category = Category;
    Event.EventId = FName(EventId);
    Event.Label = Label;
    Event.Intensity01 = SafeUnit(Intensity01);
    Event.PitchMultiplier = SafeClamp(
        PitchMultiplier,
        DiscGolfPresentationAudio::MinimumPitchMultiplier,
        DiscGolfPresentationAudio::MaximumPitchMultiplier,
        1.0f);
    return Event;
}

const TCHAR* ReleaseGradeToken(EReleaseGrade Grade)
{
    switch (Grade)
    {
        case EReleaseGrade::Perfect: return TEXT("Perfect");
        case EReleaseGrade::Great: return TEXT("Great");
        case EReleaseGrade::Good: return TEXT("Good");
        case EReleaseGrade::Poor: return TEXT("Poor");
        default: return nullptr;
    }
}

const TCHAR* ReleaseTimingToken(EReleaseTiming Timing)
{
    switch (Timing)
    {
        case EReleaseTiming::Early: return TEXT("Early");
        case EReleaseTiming::OnTime: return TEXT("OnTime");
        case EReleaseTiming::Late: return TEXT("Late");
        default: return nullptr;
    }
}

const TCHAR* ReleaseTimingLabel(EReleaseTiming Timing)
{
    switch (Timing)
    {
        case EReleaseTiming::Early: return TEXT("Early");
        case EReleaseTiming::OnTime: return TEXT("On-Time");
        case EReleaseTiming::Late: return TEXT("Late");
        default: return nullptr;
    }
}

float GradeQuality(EReleaseGrade Grade)
{
    switch (Grade)
    {
        case EReleaseGrade::Perfect: return 1.0f;
        case EReleaseGrade::Great: return 0.82f;
        case EReleaseGrade::Good: return 0.62f;
        case EReleaseGrade::Poor: return 0.32f;
        default: return 0.0f;
    }
}

const TCHAR* SurfaceToken(EGroundSurfaceType Surface)
{
    switch (Surface)
    {
        case EGroundSurfaceType::Fairway: return TEXT("Fairway");
        case EGroundSurfaceType::Rough: return TEXT("Rough");
        case EGroundSurfaceType::Dirt: return TEXT("Dirt");
        case EGroundSurfaceType::Rock: return TEXT("Rock");
        case EGroundSurfaceType::TeePad: return TEXT("TeePad");
        default: return nullptr;
    }
}

float SurfacePitch(EGroundSurfaceType Surface)
{
    switch (Surface)
    {
        case EGroundSurfaceType::Fairway: return 0.96f;
        case EGroundSurfaceType::Rough: return 0.88f;
        case EGroundSurfaceType::Dirt: return 0.99f;
        case EGroundSurfaceType::Rock: return 1.12f;
        case EGroundSurfaceType::TeePad: return 1.06f;
        default: return 1.0f;
    }
}

const TCHAR* GroundStateToken(EDiscGroundState State)
{
    switch (State)
    {
        case EDiscGroundState::Impact: return TEXT("Impact");
        case EDiscGroundState::Skipping: return TEXT("Skipping");
        case EDiscGroundState::Sliding: return TEXT("Sliding");
        case EDiscGroundState::EdgeRolling: return TEXT("EdgeRolling");
        case EDiscGroundState::Settled: return TEXT("Settled");
        case EDiscGroundState::Airborne:
        default: return nullptr;
    }
}

const TCHAR* GroundStateLabel(EDiscGroundState State)
{
    switch (State)
    {
        case EDiscGroundState::Impact: return TEXT("Ground Impact");
        case EDiscGroundState::Skipping: return TEXT("Disc Skip");
        case EDiscGroundState::Sliding: return TEXT("Disc Slide");
        case EDiscGroundState::EdgeRolling: return TEXT("Edge Roll");
        case EDiscGroundState::Settled: return TEXT("Disc Settled");
        case EDiscGroundState::Airborne:
        default: return nullptr;
    }
}

const TCHAR* CourseSurfaceToken(ECourseSurfaceType Surface)
{
    switch (Surface)
    {
        case ECourseSurfaceType::Fairway: return TEXT("Fairway");
        case ECourseSurfaceType::TeePad: return TEXT("TeePad");
        case ECourseSurfaceType::LightRough: return TEXT("LightRough");
        case ECourseSurfaceType::DeepRough: return TEXT("DeepRough");
        case ECourseSurfaceType::Dirt: return TEXT("Dirt");
        case ECourseSurfaceType::Rock: return TEXT("Rock");
        case ECourseSurfaceType::OutOfBounds: return TEXT("OutOfBounds");
        case ECourseSurfaceType::Hazard: return TEXT("HazardDry");
        default: return nullptr;
    }
}

const TCHAR* CourseSurfaceLabel(ECourseSurfaceType Surface)
{
    switch (Surface)
    {
        case ECourseSurfaceType::Fairway: return TEXT("Fairway Contact");
        case ECourseSurfaceType::TeePad: return TEXT("Tee Pad Contact");
        case ECourseSurfaceType::LightRough: return TEXT("Light Rough Contact");
        case ECourseSurfaceType::DeepRough: return TEXT("Deep Rough Contact");
        case ECourseSurfaceType::Dirt: return TEXT("Dirt Contact");
        case ECourseSurfaceType::Rock: return TEXT("Rock Contact");
        case ECourseSurfaceType::OutOfBounds: return TEXT("Out of Bounds Contact");
        case ECourseSurfaceType::Hazard: return TEXT("Dry Hazard Contact");
        default: return nullptr;
    }
}
}

bool FDiscGolfPresentationAudioEvent::IsValid() const
{
    return ContractVersion == DiscGolfPresentationAudio::CurrentContractVersion
        && Category != EDiscGolfPresentationAudioCategory::Invalid
        && !EventId.IsNone()
        && !Label.IsEmpty()
        && FMath::IsFinite(Intensity01)
        && Intensity01 >= 0.0f
        && Intensity01 <= 1.0f
        && FMath::IsFinite(PitchMultiplier)
        && PitchMultiplier >= DiscGolfPresentationAudio::MinimumPitchMultiplier
        && PitchMultiplier <= DiscGolfPresentationAudio::MaximumPitchMultiplier
        && (!bHasContext || Context.IsValid());
}

bool FDiscGolfPresentationAudioContext::IsValid() const
{
    return ShotSequence >= 0
        && HoleNumber >= 0 && HoleNumber <= 72
        && FMath::IsFinite(WorldLocationCm.X)
        && FMath::IsFinite(WorldLocationCm.Y)
        && FMath::IsFinite(WorldLocationCm.Z)
        && FMath::IsFinite(EventTimeSeconds) && EventTimeSeconds >= 0.0f;
}

FString FDiscGolfPresentationAudioEvent::DedupeKey() const
{
    if (!IsValid()) return FString();
    if (!bHasContext) return EventId.ToString();
    return FString::Printf(TEXT("%s|H%d|S%d|R%d|T%d"),
        *EventId.ToString(), Context.HoleNumber, Context.ShotSequence,
        Context.bReplayPresentation ? 1 : 0,
        FMath::RoundToInt(Context.EventTimeSeconds * 1000.0f));
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveThrowRelease(
    EReleaseGrade Grade,
    EReleaseTiming Timing,
    float Quality01,
    float ReleaseSpeedMps)
{
    const TCHAR* GradeToken = ReleaseGradeToken(Grade);
    const TCHAR* TimingToken = ReleaseTimingToken(Timing);
    const TCHAR* TimingLabel = ReleaseTimingLabel(Timing);
    const bool bValidCombination = Grade == EReleaseGrade::Perfect
        ? Timing == EReleaseTiming::OnTime
        : Timing == EReleaseTiming::Early || Timing == EReleaseTiming::Late;
    if (GradeToken == nullptr || TimingToken == nullptr || TimingLabel == nullptr || !bValidCombination) return {};

    const float BlendedQuality = 0.5f * (GradeQuality(Grade) + SafeUnit(Quality01, GradeQuality(Grade)));
    const float Speed01 = SafeRatio(ReleaseSpeedMps, 35.0f);
    const float TimingPitch = Timing == EReleaseTiming::Early
        ? -0.025f
        : Timing == EReleaseTiming::Late ? 0.025f : 0.0f;
    const FString EventId = FString::Printf(
        TEXT("Presentation.Throw.Release.%s.%s"), GradeToken, TimingToken);
    const FString Label = FString::Printf(
        TEXT("%s %s Release"), GradeToken, TimingLabel);
    return MakeEvent(
        EDiscGolfPresentationAudioCategory::ThrowRelease,
        *EventId,
        *Label,
        0.25f + 0.55f * BlendedQuality + 0.20f * Speed01,
        0.88f + 0.22f * BlendedQuality + TimingPitch);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveAirborneFlight(
    float SpeedMps,
    float SpinRpm,
    float FlightProgress01)
{
    const float Speed01 = SafeRatio(SpeedMps, 40.0f);
    const float Spin01 = SafeRatio(FMath::Abs(FMath::IsFinite(SpinRpm) ? SpinRpm : 0.0f), 1400.0f);
    const float Progress01 = SafeUnit(FlightProgress01);
    return MakeEvent(
        EDiscGolfPresentationAudioCategory::AirborneFlight,
        TEXT("Presentation.Flight.Airborne"),
        TEXT("Disc Airborne"),
        0.08f + 0.54f * Speed01 + 0.28f * Spin01 + 0.10f * (1.0f - Progress01),
        0.84f + 0.18f * Speed01 + 0.14f * Spin01);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveGroundContact(
    EGroundSurfaceType Surface,
    float ImpactSpeedMps,
    float IncidenceAngleDeg)
{
    const TCHAR* SurfaceName = SurfaceToken(Surface);
    if (SurfaceName == nullptr) return {};

    const float Speed01 = SafeRatio(ImpactSpeedMps, 30.0f);
    const float AngleDeg = FMath::Abs(FMath::IsFinite(IncidenceAngleDeg) ? IncidenceAngleDeg : 0.0f);
    const float Incidence01 = SafeRatio(AngleDeg, 90.0f);
    const FString EventId = FString::Printf(TEXT("Presentation.Ground.Contact.%s"), SurfaceName);
    const FString Label = FString::Printf(TEXT("%s Contact"), SurfaceName);
    return MakeEvent(
        EDiscGolfPresentationAudioCategory::GroundContact,
        *EventId,
        *Label,
        0.18f + 0.62f * Speed01 + 0.20f * Incidence01,
        SurfacePitch(Surface) + 0.05f * Speed01);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveCourseSurfaceContact(
    EGroundSurfaceType PhysicalSurface,
    ECourseSurfaceType CourseSurface,
    float ImpactSpeedMps,
    float IncidenceAngleDeg,
    FName PresentationMaterialId)
{
    const TCHAR* PhysicalName = SurfaceToken(PhysicalSurface);
    const bool bWater = CourseSurface == ECourseSurfaceType::Hazard
        && PresentationMaterialId == TEXT("Water");
    const TCHAR* CourseName = bWater ? TEXT("HazardWater") : CourseSurfaceToken(CourseSurface);
    const TCHAR* Label = bWater ? TEXT("Water Hazard Contact") : CourseSurfaceLabel(CourseSurface);
    if (PhysicalName == nullptr || CourseName == nullptr || Label == nullptr) return {};

    const float Speed01 = SafeRatio(ImpactSpeedMps, 30.0f);
    const float AngleDeg = FMath::Abs(FMath::IsFinite(IncidenceAngleDeg) ? IncidenceAngleDeg : 0.0f);
    const float Incidence01 = SafeRatio(AngleDeg, 90.0f);
    const FString EventId = FString::Printf(
        TEXT("Presentation.Course.Contact.%s.%s"), CourseName, PhysicalName);
    return MakeEvent(
        EDiscGolfPresentationAudioCategory::GroundContact,
        *EventId,
        Label,
        0.18f + 0.62f * Speed01 + 0.20f * Incidence01,
        SurfacePitch(PhysicalSurface) + 0.05f * Speed01);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveGroundState(
    EDiscGroundState State,
    EGroundSurfaceType Surface,
    float SpeedMps)
{
    const TCHAR* StateName = GroundStateToken(State);
    const TCHAR* StateLabel = GroundStateLabel(State);
    const TCHAR* SurfaceName = SurfaceToken(Surface);
    if (StateName == nullptr || StateLabel == nullptr || SurfaceName == nullptr) return {};

    const float Speed01 = SafeRatio(SpeedMps, 24.0f);
    const float StateBase = State == EDiscGroundState::Settled ? 0.20f : 0.30f;
    const float StatePitch = State == EDiscGroundState::EdgeRolling
        ? -0.04f
        : State == EDiscGroundState::Skipping ? 0.04f : 0.0f;
    const FString EventId = FString::Printf(
        TEXT("Presentation.Ground.State.%s.%s"), StateName, SurfaceName);
    return MakeEvent(
        EDiscGolfPresentationAudioCategory::GroundState,
        *EventId,
        StateLabel,
        StateBase + (State == EDiscGroundState::Settled ? 0.15f : 0.58f) * Speed01,
        SurfacePitch(Surface) + StatePitch + 0.03f * Speed01);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveBasketOutcome(
    EBasketContactResult Result,
    float IncomingSpeedMps)
{
    const float Speed01 = SafeRatio(IncomingSpeedMps, 18.0f);
    switch (Result)
    {
        case EBasketContactResult::Caught:
            return MakeEvent(EDiscGolfPresentationAudioCategory::BasketOutcome,
                TEXT("Presentation.Basket.Caught"), TEXT("Basket Catch"),
                0.64f + 0.36f * Speed01, 0.98f + 0.08f * Speed01);
        case EBasketContactResult::ChainDeflection:
            return MakeEvent(EDiscGolfPresentationAudioCategory::BasketOutcome,
                TEXT("Presentation.Basket.ChainDeflection"), TEXT("Chain Deflection"),
                0.48f + 0.44f * Speed01, 1.02f + 0.10f * Speed01);
        case EBasketContactResult::BandRejection:
            return MakeEvent(EDiscGolfPresentationAudioCategory::BasketOutcome,
                TEXT("Presentation.Basket.BandRejection"), TEXT("Top Band Rejection"),
                0.52f + 0.43f * Speed01, 1.08f + 0.08f * Speed01);
        case EBasketContactResult::TrayRejection:
            return MakeEvent(EDiscGolfPresentationAudioCategory::BasketOutcome,
                TEXT("Presentation.Basket.TrayRejection"), TEXT("Basket Tray Rejection"),
                0.50f + 0.42f * Speed01, 0.92f + 0.07f * Speed01);
        case EBasketContactResult::None:
        default:
            return {};
    }
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolvePenalty(
    EDiscGolfPenaltyType Penalty,
    int32 PenaltyStrokes)
{
    if (PenaltyStrokes <= 0) return {};
    const int32 SafeStrokes = FMath::Clamp(PenaltyStrokes, 1, 9);
    const float Severity01 = static_cast<float>(SafeStrokes - 1) / 8.0f;
    switch (Penalty)
    {
        case EDiscGolfPenaltyType::OutOfBounds:
            return MakeEvent(EDiscGolfPresentationAudioCategory::Penalty,
                TEXT("Presentation.Penalty.OutOfBounds"), TEXT("Out of Bounds Penalty"),
                0.62f + 0.28f * Severity01, 0.90f - 0.05f * Severity01);
        case EDiscGolfPenaltyType::Hazard:
            return MakeEvent(EDiscGolfPresentationAudioCategory::Penalty,
                TEXT("Presentation.Penalty.Hazard"), TEXT("Hazard Penalty"),
                0.58f + 0.27f * Severity01, 0.94f - 0.04f * Severity01);
        case EDiscGolfPenaltyType::None:
        default:
            return {};
    }
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveHoleStart(
    int32 HoleNumber,
    int32 TotalHoleCount,
    int32 Par)
{
    if (TotalHoleCount <= 0 || TotalHoleCount > 72
        || HoleNumber <= 0 || HoleNumber > TotalHoleCount || Par <= 0 || Par > 9)
    {
        return {};
    }
    return MakeEvent(EDiscGolfPresentationAudioCategory::HoleStart,
        TEXT("Presentation.Hole.Start"), TEXT("Hole Start"),
        0.45f + 0.25f * SafeRatio(HoleNumber, TotalHoleCount),
        0.96f + 0.02f * SafeRatio(Par, 9.0f));
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveHoleCompletion(
    int32 HoleNumber,
    int32 Strokes,
    int32 Par)
{
    if (HoleNumber <= 0 || HoleNumber > 72 || Strokes <= 0 || Strokes > 99 || Par <= 0 || Par > 9)
    {
        return {};
    }
    const int32 Relative = FMath::Clamp(Strokes - Par, -9, 9);
    const TCHAR* Result = Relative < 0 ? TEXT("UnderPar") : Relative > 0 ? TEXT("OverPar") : TEXT("EvenPar");
    const FString EventId = FString::Printf(TEXT("Presentation.Hole.Complete.%s"), Result);
    const FString Label = FString::Printf(TEXT("Hole Complete - %s"),
        Relative < 0 ? TEXT("Under Par") : Relative > 0 ? TEXT("Over Par") : TEXT("Even Par"));
    return MakeEvent(EDiscGolfPresentationAudioCategory::HoleCompletion,
        *EventId, *Label, 0.70f + 0.03f * FMath::Abs(Relative),
        Relative < 0 ? 1.06f : Relative > 0 ? 0.95f : 1.0f);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveHoleTransition(
    int32 FromHoleNumber,
    int32 ToHoleNumber,
    int32 TotalHoleCount)
{
    if (TotalHoleCount <= 0 || TotalHoleCount > 72
        || FromHoleNumber < 1 || FromHoleNumber > TotalHoleCount
        || ToHoleNumber < 1 || ToHoleNumber > TotalHoleCount
        || FromHoleNumber == ToHoleNumber)
    {
        return {};
    }

    const bool bAdvancing = ToHoleNumber > FromHoleNumber;
    return MakeEvent(
        EDiscGolfPresentationAudioCategory::HoleTransition,
        bAdvancing ? TEXT("Presentation.Hole.Transition.Advance") : TEXT("Presentation.Hole.Transition.Return"),
        bAdvancing ? TEXT("Advance to Next Hole") : TEXT("Return to Earlier Hole"),
        0.58f + 0.22f * SafeRatio(FMath::Abs(ToHoleNumber - FromHoleNumber), TotalHoleCount),
        bAdvancing ? 1.04f : 0.96f);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveRoundCompletion(
    int32 CompletedHoleCount,
    int32 TotalHoleCount,
    int32 ScoreToPar)
{
    if (TotalHoleCount <= 0 || TotalHoleCount > 72 || CompletedHoleCount != TotalHoleCount) return {};

    const int32 SafeScoreToPar = FMath::Clamp(ScoreToPar, -99, 99);
    const float ScoreMagnitude01 = SafeRatio(static_cast<float>(FMath::Abs(SafeScoreToPar)), 36.0f);
    if (ScoreToPar < 0)
    {
        return MakeEvent(EDiscGolfPresentationAudioCategory::RoundCompletion,
            TEXT("Presentation.Round.Complete.UnderPar"), TEXT("Round Complete - Under Par"),
            0.76f + 0.24f * ScoreMagnitude01, 1.08f + 0.06f * ScoreMagnitude01);
    }
    if (ScoreToPar > 0)
    {
        return MakeEvent(EDiscGolfPresentationAudioCategory::RoundCompletion,
            TEXT("Presentation.Round.Complete.OverPar"), TEXT("Round Complete - Over Par"),
            0.70f + 0.18f * ScoreMagnitude01, 0.96f - 0.06f * ScoreMagnitude01);
    }
    return MakeEvent(EDiscGolfPresentationAudioCategory::RoundCompletion,
        TEXT("Presentation.Round.Complete.EvenPar"), TEXT("Round Complete - Even Par"),
        0.80f, 1.0f);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveReplay(bool bStarting)
{
    return MakeEvent(EDiscGolfPresentationAudioCategory::Replay,
        bStarting ? TEXT("Presentation.Replay.Start") : TEXT("Presentation.Replay.Stop"),
        bStarting ? TEXT("Replay Start") : TEXT("Replay Stop"),
        bStarting ? 0.42f : 0.30f, bStarting ? 1.02f : 0.98f);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::ResolveFlyover(bool bStarting)
{
    return MakeEvent(EDiscGolfPresentationAudioCategory::Flyover,
        bStarting ? TEXT("Presentation.Flyover.Start") : TEXT("Presentation.Flyover.Stop"),
        bStarting ? TEXT("Flyover Start") : TEXT("Flyover Stop"),
        bStarting ? 0.48f : 0.32f, bStarting ? 1.03f : 0.97f);
}

FDiscGolfPresentationAudioEvent DiscGolfPresentationAudio::WithContext(
    const FDiscGolfPresentationAudioEvent& Event,
    const FDiscGolfPresentationAudioContext& Context)
{
    if (!Event.IsValid() || !Context.IsValid()) return {};
    FDiscGolfPresentationAudioEvent Result = Event;
    Result.bHasContext = true;
    Result.Context = Context;
    return Result;
}

bool DiscGolfPresentationAudio::ShouldEmit(
    const FDiscGolfPresentationAudioEvent& Event,
    bool bRegressionActive)
{
    return Event.IsValid() && !bRegressionActive;
}

bool DiscGolfPresentationAudio::AppendBoundedTrace(
    TArray<FDiscGolfPresentationAudioEvent>& Trace,
    const FDiscGolfPresentationAudioEvent& Event,
    int32 MaximumEvents)
{
    if (!Event.IsValid() || MaximumEvents <= 0) return false;
    const FString Key = Event.DedupeKey();
    if (Trace.Num() > 0 && Trace.Last().DedupeKey() == Key) return false;
    Trace.Add(Event);
    const int32 SafeMaximum = FMath::Clamp(MaximumEvents, 1, 1024);
    if (Trace.Num() > SafeMaximum)
    {
        Trace.RemoveAt(0, Trace.Num() - SafeMaximum, EAllowShrinking::No);
    }
    return true;
}
