#include "DiscThrowLabSubsystem.h"

#include "DiscGolfPresentationMath.h"
#include "DiscThrowLabSaveGame.h"
#include "Kismet/GameplayStatics.h"

bool UDiscThrowLabSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return DG_DEVELOPER_WITH_THROW_LAB != 0 && Super::ShouldCreateSubsystem(Outer);
}

namespace
{
bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
}

bool IsFiniteAero(const FDiscAeroProfile& Aero)
{
    return FMath::IsFinite(Aero.MassKg)
        && FMath::IsFinite(Aero.DiameterM)
        && FMath::IsFinite(Aero.AreaM2)
        && FMath::IsFinite(Aero.InertiaAxialKgM2)
        && FMath::IsFinite(Aero.InertiaPlanarKgM2)
        && FMath::IsFinite(Aero.CL0)
        && FMath::IsFinite(Aero.CLa)
        && FMath::IsFinite(Aero.CD0)
        && FMath::IsFinite(Aero.CDa)
        && FMath::IsFinite(Aero.CM0)
        && FMath::IsFinite(Aero.CMa)
        && FMath::IsFinite(Aero.HighSpeedTurnMomentNm)
        && FMath::IsFinite(Aero.LowSpeedFadeMomentNm)
        && FMath::IsFinite(Aero.TurnStartsAboveMps)
        && FMath::IsFinite(Aero.FadeStartsBelowMps)
        && FMath::IsFinite(Aero.SpinDecayPerSecond)
        && FMath::IsFinite(Aero.GroundRestitution)
        && FMath::IsFinite(Aero.GroundFriction)
        && Aero.MassKg > 0.0f
        && Aero.DiameterM > 0.0f
        && Aero.AreaM2 > 0.0f
        && Aero.InertiaAxialKgM2 > 0.0f
        && Aero.InertiaPlanarKgM2 > 0.0f;
}

template <typename T>
bool IsValidEnum(T Value)
{
    const UEnum* Enum = StaticEnum<T>();
    return Enum && Enum->IsValidEnumValue(static_cast<int64>(Value));
}

bool IsStableCaptureId(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 128) return false;
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsAlnum(Character)
            && Character != TEXT('_')
            && Character != TEXT('-')
            && Character != TEXT('.'))
        {
            return false;
        }
    }
    return true;
}

bool IsFiniteRelease(const FThrowRelease& Release)
{
    return IsValidEnum(Release.Grade)
        && IsValidEnum(Release.Timing)
        && IsValidEnum(Release.ThrowStyle)
        && IsValidEnum(Release.ShotContext)
        && FMath::IsFinite(Release.TimingError)
        && FMath::IsFinite(Release.Quality01)
        && FMath::IsFinite(Release.SpeedMultiplier)
        && FMath::IsFinite(Release.SpinMultiplier)
        && FMath::IsFinite(Release.ReleaseSpeedMps)
        && FMath::IsFinite(Release.SpinRpm)
        && FMath::IsFinite(Release.AimOffsetDeg)
        && FMath::IsFinite(Release.HyzerOffsetDeg)
        && FMath::IsFinite(Release.NoseOffsetDeg)
        && FMath::IsFinite(Release.LaunchOffsetDeg)
        && FMath::IsFinite(Release.EffectiveHyzerDeg)
        && FMath::IsFinite(Release.EffectiveNoseAngleDeg)
        && FMath::IsFinite(Release.EffectiveLaunchAngleDeg)
        && FMath::IsFinite(Release.LiePowerMultiplier)
        && FMath::IsFinite(Release.LieTimingErrorMultiplier)
        && IsFiniteVector(Release.Direction)
        && !Release.Direction.IsNearlyZero()
        && Release.ReleaseSpeedMps >= 0.0f
        && Release.SpinRpm >= 0.0f;
}

bool IsFiniteSample(const FDiscTrajectorySample& Sample)
{
    return FMath::IsFinite(Sample.TimeSeconds)
        && IsFiniteVector(Sample.WorldLocationCm)
        && IsFiniteVector(Sample.VelocityMps)
        && IsFiniteVector(Sample.DiscNormalWorld)
        && IsFiniteVector(Sample.WindMps)
        && FMath::IsFinite(Sample.SpinRpm)
        && FMath::IsFinite(Sample.AngleOfAttackDeg)
        && IsValidEnum(Sample.GroundState)
        && IsValidEnum(Sample.GroundSurface)
        && IsValidEnum(Sample.CourseSurface)
        && Sample.TimeSeconds >= 0.0f
        && Sample.SpinRpm >= 0.0f
        && Sample.GroundContactCount >= 0;
}

bool IsFiniteTransition(const FDiscGroundTransition& Transition)
{
    return FMath::IsFinite(Transition.TimeSeconds)
        && IsFiniteVector(Transition.WorldLocationCm)
        && IsValidEnum(Transition.FromState)
        && IsValidEnum(Transition.ToState)
        && IsValidEnum(Transition.Surface)
        && IsValidEnum(Transition.CourseSurface)
        && FMath::IsFinite(Transition.ImpactSpeedMps)
        && FMath::IsFinite(Transition.IncidenceAngleDeg)
        && FMath::IsFinite(Transition.DiscEdgeAngleDeg)
        && Transition.TimeSeconds >= 0.0f
        && Transition.GroundContactCount >= 0
        && Transition.ImpactSpeedMps >= 0.0f;
}

bool IsFiniteTelemetry(const FDiscFlightTelemetry& Telemetry)
{
    return IsValidEnum(Telemetry.State)
        && IsValidEnum(Telemetry.GroundState)
        && IsValidEnum(Telemetry.GroundSurface)
        && IsValidEnum(Telemetry.CourseSurface)
        && IsValidEnum(Telemetry.LastFixtureType)
        && IsValidEnum(Telemetry.LastBasketContact)
        && IsFiniteVector(Telemetry.VelocityMps)
        && IsFiniteVector(Telemetry.LastFixtureEntryVelocityMps)
        && IsFiniteVector(Telemetry.LastFixtureExitVelocityMps)
        && IsFiniteVector(Telemetry.LastFixtureImpactNormal)
        && FMath::IsFinite(Telemetry.SpeedMps)
        && FMath::IsFinite(Telemetry.SpinRpm)
        && FMath::IsFinite(Telemetry.AngleOfAttackDeg)
        && FMath::IsFinite(Telemetry.FlightTimeSeconds)
        && FMath::IsFinite(Telemetry.CarryMeters)
        && FMath::IsFinite(Telemetry.GroundPlayTimeSeconds)
        && FMath::IsFinite(Telemetry.GroundDistanceMeters)
        && FMath::IsFinite(Telemetry.LastImpactSpeedMps)
        && FMath::IsFinite(Telemetry.LastImpactIncidenceDeg)
        && FMath::IsFinite(Telemetry.LastDiscEdgeAngleDeg)
        && FMath::IsFinite(Telemetry.LastFixtureImpactSpeedMps)
        && FMath::IsFinite(Telemetry.LastFixtureEntrySpinRpm)
        && FMath::IsFinite(Telemetry.LastFixtureExitSpinRpm)
        && Telemetry.SpeedMps >= 0.0f
        && Telemetry.SpinRpm >= 0.0f
        && Telemetry.FlightTimeSeconds >= 0.0f
        && Telemetry.GroundContactCount >= 0
        && Telemetry.FixtureContactCount >= 0
        && Telemetry.BasketContactCount >= 0
        && IsFiniteRelease(Telemetry.Release);
}

bool IsFiniteSummary(const FDiscTrajectorySummary& Summary)
{
    return FMath::IsFinite(Summary.DurationSeconds)
        && FMath::IsFinite(Summary.AirTimeSeconds)
        && FMath::IsFinite(Summary.AirCarryMeters)
        && FMath::IsFinite(Summary.FinalCarryMeters)
        && FMath::IsFinite(Summary.ApexMeters)
        && FMath::IsFinite(Summary.LateralMeters)
        && IsFiniteVector(Summary.StartWorldLocationCm)
        && IsFiniteVector(Summary.FinalWorldLocationCm)
        && FMath::IsFinite(Summary.GroundDistanceMeters)
        && IsFiniteVector(Summary.ResultingLieLocationCm)
        && IsValidEnum(Summary.FinalGroundState)
        && IsValidEnum(Summary.FinalGroundSurface)
        && IsValidEnum(Summary.LastBasketContact)
        && IsValidEnum(Summary.SurfaceAtRest)
        && IsValidEnum(Summary.PlayingSurface)
        && IsValidEnum(Summary.ResultingLieType)
        && IsValidEnum(Summary.PenaltyType)
        && IsValidEnum(Summary.ReliefRule)
        && Summary.SampleCount >= 0
        && Summary.GroundTransitionCount >= 0
        && Summary.DurationSeconds >= 0.0f
        && Summary.AirTimeSeconds >= 0.0f
        && Summary.ApexMeters >= 0.0f
        && Summary.GroundContactCount >= 0
        && Summary.GroundDistanceMeters >= 0.0f
        && Summary.BasketContactCount >= 0
        && Summary.PenaltyStrokes >= 0
        && Summary.RegressionFailures.Num() <= 64;
}

bool SameReleaseIdentity(const FThrowRelease& First, const FThrowRelease& Second)
{
    return First.ThrowStyle == Second.ThrowStyle
        && First.ShotContext == Second.ShotContext
        && FMath::IsNearlyEqual(First.ReleaseSpeedMps, Second.ReleaseSpeedMps)
        && FMath::IsNearlyEqual(First.SpinRpm, Second.SpinRpm)
        && First.Direction.Equals(Second.Direction, KINDA_SMALL_NUMBER);
}

bool ValidateCaptureInputs(
    const FResolvedDiscDefinition& Disc,
    const FThrowRelease& Release,
    const TArray<FDiscTrajectorySample>& Samples,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscFlightTelemetry& FinalTelemetry,
    const FDiscTrajectorySummary& Summary,
    FString& OutError)
{
    if (!IsStableCaptureId(Summary.CaptureId))
    {
        OutError = TEXT("Throw Lab requires a stable alphanumeric trajectory capture ID");
        return false;
    }
    FDateTime ParsedCapturedUtc;
    if (Summary.CapturedUtc.Len() > 64
        || !FDateTime::ParseIso8601(*Summary.CapturedUtc, ParsedCapturedUtc))
    {
        OutError = TEXT("Throw Lab requires the capture's ISO-8601 UTC timestamp");
        return false;
    }
    if (Disc.MoldId.IsNone() || Disc.MoldId.ToString().Len() > 64
        || Disc.DisplayName.ToString().Len() > 128
        || !IsValidEnum(Disc.Plastic)
        || !FMath::IsFinite(Disc.Turn)
        || !FMath::IsFinite(Disc.Fade)
        || !IsFiniteAero(Disc.Aero))
    {
        OutError = TEXT("Throw Lab received an invalid or non-finite resolved disc");
        return false;
    }
    if (!IsFiniteRelease(Release))
    {
        OutError = TEXT("Throw Lab received an invalid or non-finite immutable release");
        return false;
    }
    if (Samples.Num() < 2 || Samples.Num() > UDiscThrowLabSubsystem::MaxSourceSamples)
    {
        OutError = FString::Printf(TEXT("Throw Lab requires 2..%d source samples"),
            UDiscThrowLabSubsystem::MaxSourceSamples);
        return false;
    }
    float PreviousTime = -1.0f;
    for (const FDiscTrajectorySample& Sample : Samples)
    {
        if (!IsFiniteSample(Sample) || Sample.TimeSeconds + KINDA_SMALL_NUMBER < PreviousTime)
        {
            OutError = TEXT("Throw Lab source samples must be finite and time ordered");
            return false;
        }
        PreviousTime = Sample.TimeSeconds;
    }
    const float CaptureDuration = Samples.Last().TimeSeconds - Samples[0].TimeSeconds;
    if (!FMath::IsFinite(CaptureDuration)
        || CaptureDuration < 0.0f
        || CaptureDuration > UDiscThrowLabSubsystem::MaxCaptureDurationSeconds)
    {
        OutError = TEXT("Throw Lab source duration is outside the bounded capture window");
        return false;
    }
    if (Transitions.Num() > UDiscThrowLabSubsystem::MaxGroundTransitions)
    {
        OutError = FString::Printf(TEXT("Throw Lab supports at most %d discrete ground transitions"),
            UDiscThrowLabSubsystem::MaxGroundTransitions);
        return false;
    }
    PreviousTime = -1.0f;
    for (const FDiscGroundTransition& Transition : Transitions)
    {
        if (!IsFiniteTransition(Transition)
            || Transition.TimeSeconds + KINDA_SMALL_NUMBER < PreviousTime
            || Transition.TimeSeconds + KINDA_SMALL_NUMBER < Samples[0].TimeSeconds
            || Transition.TimeSeconds > Samples.Last().TimeSeconds + KINDA_SMALL_NUMBER)
        {
            OutError = TEXT("Throw Lab transitions must be finite, ordered, and inside the sample window");
            return false;
        }
        PreviousTime = Transition.TimeSeconds;
    }
    if (!IsFiniteTelemetry(FinalTelemetry)
        || (FinalTelemetry.State != EDiscFlightState::Settled
            && FinalTelemetry.State != EDiscFlightState::HoledOut)
        || !SameReleaseIdentity(Release, FinalTelemetry.Release))
    {
        OutError = TEXT("Throw Lab only accepts finite final telemetry from this completed release");
        return false;
    }
    if (!IsFiniteSummary(Summary)
        || Summary.SampleCount != Samples.Num()
        || Summary.GroundTransitionCount != Transitions.Num()
        || Summary.FinalGroundState != FinalTelemetry.GroundState
        || Summary.StartWorldLocationCm.Equals(Samples[0].WorldLocationCm, 0.01f) == false
        || Summary.FinalWorldLocationCm.Equals(Samples.Last().WorldLocationCm, 0.01f) == false)
    {
        OutError = TEXT("Throw Lab summary does not identify the completed authoritative capture");
        return false;
    }
    for (const FString& Failure : Summary.RegressionFailures)
    {
        if (Failure.Len() > 256)
        {
            OutError = TEXT("Throw Lab regression failure text exceeds the bounded record schema");
            return false;
        }
    }
    OutError.Reset();
    return true;
}
}

bool UDiscThrowLabSubsystem::BuildBoundedReplaySamples(
    const TArray<FDiscTrajectorySample>& Source,
    const TArray<FDiscGroundTransition>& Transitions,
    TArray<FDiscTrajectorySample>& OutSamples,
    FString& OutError)
{
    FDiscActualReplaySelectionPolicy Policy;
    Policy.MaxSourceSamples = MaxSourceSamples;
    Policy.MaxOutputSamples = MaxReplaySamples;
    Policy.MaxSampleRateHz = MaxReplaySampleRateHz;
    Policy.MaxDurationSeconds = MaxCaptureDurationSeconds;
    return DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
        Source, Transitions, Policy, OutSamples, OutError);
}

bool UDiscThrowLabSubsystem::BuildRecord(
    const FResolvedDiscDefinition& Disc,
    const FThrowRelease& Release,
    const TArray<FDiscTrajectorySample>& Samples,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscFlightTelemetry& FinalTelemetry,
    const FDiscTrajectorySummary& Summary,
    FDiscThrowLabRecord& OutRecord,
    FString& OutError)
{
    OutRecord = FDiscThrowLabRecord();
    if (!ValidateCaptureInputs(Disc, Release, Samples, Transitions, FinalTelemetry, Summary, OutError))
    {
        return false;
    }

    TArray<FDiscTrajectorySample> ReplaySamples;
    if (!BuildBoundedReplaySamples(Samples, Transitions, ReplaySamples, OutError))
    {
        return false;
    }

    OutRecord.SchemaVersion = RecordSchemaVersion;
    OutRecord.RecordId = Summary.CaptureId;
    OutRecord.CaptureId = Summary.CaptureId;
    OutRecord.CapturedUtc = Summary.CapturedUtc;
    OutRecord.Disc = Disc;
    OutRecord.Release = Release;
    OutRecord.FinalTelemetry = FinalTelemetry;
    OutRecord.Summary = Summary;
    OutRecord.GroundTransitions = Transitions;
    OutRecord.ReplaySamples = MoveTemp(ReplaySamples);
    OutRecord.SourceSampleCount = Samples.Num();
    OutRecord.ReplayDurationSeconds =
        OutRecord.ReplaySamples.Last().TimeSeconds - OutRecord.ReplaySamples[0].TimeSeconds;
    OutRecord.NominalReplaySampleRateHz = OutRecord.ReplayDurationSeconds > KINDA_SMALL_NUMBER
        ? static_cast<float>(OutRecord.ReplaySamples.Num() - 1) / OutRecord.ReplayDurationSeconds
        : 0.0f;

    return ValidateRecord(OutRecord, OutError);
}

bool UDiscThrowLabSubsystem::ValidateRecord(const FDiscThrowLabRecord& Record, FString& OutError)
{
    FDateTime ParsedCapturedUtc;
    if (Record.SchemaVersion != RecordSchemaVersion
        || !IsStableCaptureId(Record.RecordId)
        || Record.RecordId != Record.CaptureId
        || Record.CaptureId != Record.Summary.CaptureId
        || Record.CapturedUtc != Record.Summary.CapturedUtc
        || Record.CapturedUtc.Len() > 64
        || !FDateTime::ParseIso8601(*Record.CapturedUtc, ParsedCapturedUtc))
    {
        OutError = TEXT("Throw Lab record identity or schema is invalid");
        return false;
    }
    if (Record.SourceSampleCount < Record.ReplaySamples.Num()
        || Record.SourceSampleCount > MaxSourceSamples
        || Record.ReplaySamples.Num() < 2
        || Record.ReplaySamples.Num() > MaxReplaySamples
        || Record.GroundTransitions.Num() > MaxGroundTransitions
        || Record.Summary.SampleCount != Record.SourceSampleCount
        || Record.Summary.GroundTransitionCount != Record.GroundTransitions.Num())
    {
        OutError = TEXT("Throw Lab record exceeds its source, replay, or transition bounds");
        return false;
    }
    if (!IsFiniteAero(Record.Disc.Aero)
        || Record.Disc.MoldId.IsNone()
        || Record.Disc.MoldId.ToString().Len() > 64
        || Record.Disc.DisplayName.ToString().Len() > 128
        || !IsValidEnum(Record.Disc.Plastic)
        || !FMath::IsFinite(Record.Disc.Turn)
        || !FMath::IsFinite(Record.Disc.Fade)
        || !IsFiniteRelease(Record.Release)
        || !IsFiniteTelemetry(Record.FinalTelemetry)
        || !IsFiniteSummary(Record.Summary)
        || (Record.FinalTelemetry.State != EDiscFlightState::Settled
            && Record.FinalTelemetry.State != EDiscFlightState::HoledOut)
        || !SameReleaseIdentity(Record.Release, Record.FinalTelemetry.Release))
    {
        OutError = TEXT("Throw Lab record contains invalid disc, release, telemetry, or summary data");
        return false;
    }
    for (const FString& Failure : Record.Summary.RegressionFailures)
    {
        if (Failure.Len() > 256)
        {
            OutError = TEXT("Throw Lab record contains oversized regression failure text");
            return false;
        }
    }
    if (Record.PositionUnit != TEXT("centimeter")
        || Record.VelocityUnit != TEXT("meter_per_second")
        || Record.SpinUnit != TEXT("revolution_per_minute"))
    {
        OutError = TEXT("Throw Lab record unit contract is invalid");
        return false;
    }

    float PreviousTime = -1.0f;
    for (const FDiscTrajectorySample& Sample : Record.ReplaySamples)
    {
        if (!IsFiniteSample(Sample) || Sample.TimeSeconds + KINDA_SMALL_NUMBER < PreviousTime)
        {
            OutError = TEXT("Throw Lab replay samples are invalid or unordered");
            return false;
        }
        PreviousTime = Sample.TimeSeconds;
    }
    if (!Record.Summary.StartWorldLocationCm.Equals(Record.ReplaySamples[0].WorldLocationCm, 0.01f)
        || !Record.Summary.FinalWorldLocationCm.Equals(Record.ReplaySamples.Last().WorldLocationCm, 0.01f))
    {
        OutError = TEXT("Throw Lab replay does not retain the capture endpoints");
        return false;
    }
    const float Duration = Record.ReplaySamples.Last().TimeSeconds - Record.ReplaySamples[0].TimeSeconds;
    const float NominalRate = Duration > KINDA_SMALL_NUMBER
        ? static_cast<float>(Record.ReplaySamples.Num() - 1) / Duration
        : 0.0f;
    if (!FMath::IsFinite(Record.ReplayDurationSeconds)
        || !FMath::IsFinite(Record.NominalReplaySampleRateHz)
        || !FMath::IsNearlyEqual(Duration, Record.ReplayDurationSeconds, KINDA_SMALL_NUMBER)
        || !FMath::IsNearlyEqual(NominalRate, Record.NominalReplaySampleRateHz, 0.01f)
        || NominalRate > MaxReplaySampleRateHz + 0.01f)
    {
        OutError = TEXT("Throw Lab replay violates its deterministic 60 Hz duration contract");
        return false;
    }

    PreviousTime = -1.0f;
    for (const FDiscGroundTransition& Transition : Record.GroundTransitions)
    {
        if (!IsFiniteTransition(Transition)
            || Transition.TimeSeconds + KINDA_SMALL_NUMBER < PreviousTime
            || Transition.TimeSeconds + KINDA_SMALL_NUMBER < Record.ReplaySamples[0].TimeSeconds
            || Transition.TimeSeconds > Record.ReplaySamples.Last().TimeSeconds + KINDA_SMALL_NUMBER)
        {
            OutError = TEXT("Throw Lab ground transitions are invalid or unordered");
            return false;
        }
        PreviousTime = Transition.TimeSeconds;
    }
    OutError.Reset();
    return true;
}

bool UDiscThrowLabSubsystem::RecordCompletedThrow(
    const FResolvedDiscDefinition& Disc,
    const FThrowRelease& Release,
    const TArray<FDiscTrajectorySample>& Samples,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscFlightTelemetry& FinalTelemetry,
    const FDiscTrajectorySummary& Summary,
    FString& OutError)
{
    if (FindRecordIndex(Summary.CaptureId) != INDEX_NONE)
    {
        OutError = FString::Printf(TEXT("Throw Lab already contains capture %s"), *Summary.CaptureId);
        return false;
    }

    FDiscThrowLabRecord Record;
    if (!BuildRecord(Disc, Release, Samples, Transitions, FinalTelemetry, Summary, Record, OutError))
    {
        return false;
    }

    if (Records.Num() >= MaxRecords)
    {
        const int32 EvictionIndex = Records.IndexOfByPredicate([](const FDiscThrowLabRecord& Candidate)
        {
            return !Candidate.bPinned;
        });
        if (EvictionIndex == INDEX_NONE)
        {
            OutError = FString::Printf(
                TEXT("Throw Lab is full (%d records) and every record is pinned"), MaxRecords);
            return false;
        }
        const FString EvictedId = Records[EvictionIndex].RecordId;
        Records.RemoveAt(EvictionIndex);
        ClearComparisonIfRecordReferenced(EvictedId);
    }

    SelectedRecordId = Record.RecordId;
    Records.Add(MoveTemp(Record));
    OutError.Reset();
    return true;
}

int32 UDiscThrowLabSubsystem::FindRecordIndex(const FString& RecordId) const
{
    return Records.IndexOfByPredicate([&RecordId](const FDiscThrowLabRecord& Record)
    {
        return Record.RecordId == RecordId;
    });
}

int32 UDiscThrowLabSubsystem::GetSelectedRecordIndex() const
{
    return FindRecordIndex(SelectedRecordId);
}

const FDiscThrowLabRecord* UDiscThrowLabSubsystem::GetRecordByIndex(int32 Index) const
{
    return Records.IsValidIndex(Index) ? &Records[Index] : nullptr;
}

const FDiscThrowLabRecord* UDiscThrowLabSubsystem::GetSelectedRecord() const
{
    return GetRecordByIndex(GetSelectedRecordIndex());
}

bool UDiscThrowLabSubsystem::GetSelectedRecordCopy(FDiscThrowLabRecord& OutRecord) const
{
    if (const FDiscThrowLabRecord* Record = GetSelectedRecord())
    {
        OutRecord = *Record;
        return true;
    }
    OutRecord = FDiscThrowLabRecord();
    return false;
}

bool UDiscThrowLabSubsystem::SelectRecordByIndex(int32 Index, FString& OutError)
{
    if (!Records.IsValidIndex(Index))
    {
        OutError = FString::Printf(TEXT("Throw Lab record index %d is out of range"), Index);
        return false;
    }
    SelectedRecordId = Records[Index].RecordId;
    OutError.Reset();
    return true;
}

bool UDiscThrowLabSubsystem::SelectRecordByOffset(int32 Offset, FString& OutError)
{
    if (Records.IsEmpty())
    {
        OutError = TEXT("Throw Lab has no records to select");
        return false;
    }
    const int64 CurrentIndex = FMath::Max(0, GetSelectedRecordIndex());
    const int64 Count = Records.Num();
    const int32 WrappedIndex = static_cast<int32>(((CurrentIndex + Offset) % Count + Count) % Count);
    return SelectRecordByIndex(WrappedIndex, OutError);
}

bool UDiscThrowLabSubsystem::ToggleSelectedPinned(bool& bOutPinned, FString& OutError)
{
    const int32 Index = GetSelectedRecordIndex();
    if (!Records.IsValidIndex(Index))
    {
        bOutPinned = false;
        OutError = TEXT("Throw Lab has no selected record to pin");
        return false;
    }
    Records[Index].bPinned = !Records[Index].bPinned;
    bOutPinned = Records[Index].bPinned;
    OutError.Reset();
    return true;
}

void UDiscThrowLabSubsystem::ClearComparisonIfRecordReferenced(const FString& RecordId)
{
    if (Comparison.FirstRecordId == RecordId || Comparison.SecondRecordId == RecordId)
    {
        ClearComparison();
    }
}

bool UDiscThrowLabSubsystem::DeleteSelected(FString& OutError)
{
    const int32 Index = GetSelectedRecordIndex();
    if (!Records.IsValidIndex(Index))
    {
        OutError = TEXT("Throw Lab has no selected record to delete");
        return false;
    }
    const FString DeletedId = Records[Index].RecordId;
    Records.RemoveAt(Index);
    ClearComparisonIfRecordReferenced(DeletedId);
    SelectedRecordId = Records.IsEmpty()
        ? FString()
        : Records[FMath::Min(Index, Records.Num() - 1)].RecordId;
    OutError.Reset();
    return true;
}

FDiscThrowLabComparison UDiscThrowLabSubsystem::CompareRecords(
    const FDiscThrowLabRecord* First,
    const FDiscThrowLabRecord* Second)
{
    FDiscThrowLabComparison Result;
    Result.FirstRecordId = First ? First->RecordId : FString();
    Result.SecondRecordId = Second ? Second->RecordId : FString();
    if (!First || !Second)
    {
        Result.Compatibility = EDiscThrowLabComparisonCompatibility::MissingRecord;
        Result.Reason = TEXT("comparison requires exactly two existing records");
        return Result;
    }
    if (First->RecordId == Second->RecordId)
    {
        Result.Compatibility = EDiscThrowLabComparisonCompatibility::SameRecord;
        Result.Reason = TEXT("comparison records must be distinct");
        return Result;
    }
    if (First->SchemaVersion != Second->SchemaVersion)
    {
        Result.Compatibility = EDiscThrowLabComparisonCompatibility::SchemaMismatch;
        Result.Reason = TEXT("record schemas differ");
        return Result;
    }
    FString ValidationError;
    if (!ValidateRecord(*First, ValidationError) || !ValidateRecord(*Second, ValidationError))
    {
        Result.Compatibility = EDiscThrowLabComparisonCompatibility::InvalidRecord;
        Result.Reason = ValidationError;
        return Result;
    }
    if (First->Release.ShotContext != Second->Release.ShotContext)
    {
        Result.Compatibility = EDiscThrowLabComparisonCompatibility::ShotContextMismatch;
        Result.Reason = TEXT("drive and putting captures are not directly comparable");
        return Result;
    }
    if (First->Summary.bWasRegression != Second->Summary.bWasRegression)
    {
        Result.Compatibility = EDiscThrowLabComparisonCompatibility::RegressionModeMismatch;
        Result.Reason = TEXT("regression and player captures use different comparison contexts");
        return Result;
    }

    Result.Compatibility = EDiscThrowLabComparisonCompatibility::Compatible;
    Result.Reason = TEXT("compatible schema, units, and shot context");
    Result.ReleaseSpeedDeltaMps = Second->Release.ReleaseSpeedMps - First->Release.ReleaseSpeedMps;
    Result.ReleaseSpinDeltaRpm = Second->Release.SpinRpm - First->Release.SpinRpm;
    Result.AirCarryDeltaMeters = Second->Summary.AirCarryMeters - First->Summary.AirCarryMeters;
    Result.FinalCarryDeltaMeters = Second->Summary.FinalCarryMeters - First->Summary.FinalCarryMeters;
    Result.ApexDeltaMeters = Second->Summary.ApexMeters - First->Summary.ApexMeters;
    Result.AirTimeDeltaSeconds = Second->Summary.AirTimeSeconds - First->Summary.AirTimeSeconds;
    Result.LateralDeltaMeters = Second->Summary.LateralMeters - First->Summary.LateralMeters;
    Result.GroundPlayDeltaMeters =
        Second->Summary.GroundDistanceMeters - First->Summary.GroundDistanceMeters;
    return Result;
}

bool UDiscThrowLabSubsystem::SetComparisonByIndices(
    int32 FirstIndex,
    int32 SecondIndex,
    FString& OutError)
{
    Comparison = CompareRecords(GetRecordByIndex(FirstIndex), GetRecordByIndex(SecondIndex));
    if (Comparison.Compatibility != EDiscThrowLabComparisonCompatibility::Compatible)
    {
        OutError = Comparison.Reason;
        return false;
    }
    OutError.Reset();
    return true;
}

void UDiscThrowLabSubsystem::ClearComparison()
{
    Comparison = FDiscThrowLabComparison();
}

void UDiscThrowLabSubsystem::ClearLab()
{
    Records.Reset();
    SelectedRecordId.Reset();
    ClearComparison();
}

FString UDiscThrowLabSubsystem::GetStatusText() const
{
    const FDiscThrowLabRecord* Selected = GetSelectedRecord();
    if (!Selected)
    {
        return FString::Printf(TEXT("THROW LAB 0/%d - no completed captures"), MaxRecords);
    }
    return FString::Printf(
        TEXT("THROW LAB %d/%d - %s%s - replay %d/%d @ %.1f Hz"),
        Records.Num(), MaxRecords, *Selected->RecordId,
        Selected->bPinned ? TEXT(" [PINNED]") : TEXT(""),
        Selected->ReplaySamples.Num(), Selected->SourceSampleCount,
        Selected->NominalReplaySampleRateHz);
}

FString UDiscThrowLabSubsystem::GetComparisonText() const
{
    if (Comparison.Compatibility == EDiscThrowLabComparisonCompatibility::NotConfigured)
    {
        return TEXT("COMPARE - select exactly two completed records");
    }
    if (Comparison.Compatibility != EDiscThrowLabComparisonCompatibility::Compatible)
    {
        return FString::Printf(TEXT("COMPARE INCOMPATIBLE - %s"), *Comparison.Reason);
    }
    return FString::Printf(
        TEXT("COMPARE %s -> %s | speed %+.2f m/s | spin %+.0f rpm | carry %+.2f m | apex %+.2f m"),
        *Comparison.FirstRecordId, *Comparison.SecondRecordId,
        Comparison.ReleaseSpeedDeltaMps, Comparison.ReleaseSpinDeltaRpm,
        Comparison.FinalCarryDeltaMeters, Comparison.ApexDeltaMeters);
}

FString UDiscThrowLabSubsystem::DefaultSaveSlot()
{
    return TEXT("DGT_ThrowLab_Dev_v1");
}

bool UDiscThrowLabSubsystem::SaveLab(FString& OutError) const
{
    return SaveLabToSlot(DefaultSaveSlot(), 0, OutError);
}

bool UDiscThrowLabSubsystem::LoadLab(FString& OutError)
{
    return LoadLabFromSlot(DefaultSaveSlot(), 0, OutError);
}

bool UDiscThrowLabSubsystem::SaveLabToSlot(
    const FString& SlotName,
    int32 UserIndex,
    FString& OutError) const
{
    if (SlotName.IsEmpty() || UserIndex < 0)
    {
        OutError = TEXT("Throw Lab save slot and user index are invalid");
        return false;
    }
    for (const FDiscThrowLabRecord& Record : Records)
    {
        if (!ValidateRecord(Record, OutError)) return false;
    }

    UDiscThrowLabSaveGame* Save = Cast<UDiscThrowLabSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UDiscThrowLabSaveGame::StaticClass()));
    if (!Save)
    {
        OutError = TEXT("Throw Lab could not allocate its schema-v1 save container");
        return false;
    }
    Save->Records = Records;
    Save->SelectedRecordId = SelectedRecordId;
    if (Comparison.Compatibility == EDiscThrowLabComparisonCompatibility::Compatible)
    {
        Save->FirstComparisonRecordId = Comparison.FirstRecordId;
        Save->SecondComparisonRecordId = Comparison.SecondRecordId;
    }
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex))
    {
        OutError = FString::Printf(TEXT("Throw Lab failed to write save slot %s"), *SlotName);
        return false;
    }
    OutError.Reset();
    return true;
}

bool UDiscThrowLabSubsystem::LoadLabFromSlot(
    const FString& SlotName,
    int32 UserIndex,
    FString& OutError)
{
    if (SlotName.IsEmpty() || UserIndex < 0)
    {
        OutError = TEXT("Throw Lab save slot and user index are invalid");
        return false;
    }
    UDiscThrowLabSaveGame* Save = Cast<UDiscThrowLabSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
    if (!Save || Save->SchemaVersion != UDiscThrowLabSaveGame::CurrentSchemaVersion)
    {
        OutError = TEXT("Throw Lab save is missing or is not schema v1");
        return false;
    }
    if (Save->Records.Num() > MaxRecords)
    {
        OutError = TEXT("Throw Lab save exceeds the bounded record count");
        return false;
    }

    TSet<FString> UniqueIds;
    for (const FDiscThrowLabRecord& Record : Save->Records)
    {
        if (!ValidateRecord(Record, OutError)) return false;
        if (UniqueIds.Contains(Record.RecordId))
        {
            OutError = TEXT("Throw Lab save contains duplicate capture identities");
            return false;
        }
        UniqueIds.Add(Record.RecordId);
    }
    if (!Save->SelectedRecordId.IsEmpty() && !UniqueIds.Contains(Save->SelectedRecordId))
    {
        OutError = TEXT("Throw Lab save selected record is missing");
        return false;
    }

    FDiscThrowLabComparison LoadedComparison;
    const bool bHasFirst = !Save->FirstComparisonRecordId.IsEmpty();
    const bool bHasSecond = !Save->SecondComparisonRecordId.IsEmpty();
    if (bHasFirst != bHasSecond)
    {
        OutError = TEXT("Throw Lab save comparison must contain exactly two record identities");
        return false;
    }
    if (bHasFirst)
    {
        const FDiscThrowLabRecord* First = Save->Records.FindByPredicate(
            [Save](const FDiscThrowLabRecord& Record)
            {
                return Record.RecordId == Save->FirstComparisonRecordId;
            });
        const FDiscThrowLabRecord* Second = Save->Records.FindByPredicate(
            [Save](const FDiscThrowLabRecord& Record)
            {
                return Record.RecordId == Save->SecondComparisonRecordId;
            });
        LoadedComparison = CompareRecords(First, Second);
        if (LoadedComparison.Compatibility != EDiscThrowLabComparisonCompatibility::Compatible)
        {
            OutError = FString::Printf(TEXT("Throw Lab saved comparison is incompatible: %s"),
                *LoadedComparison.Reason);
            return false;
        }
    }

    // Commit only after the entire separate save has been validated.
    Records = Save->Records;
    SelectedRecordId = Save->SelectedRecordId;
    if (SelectedRecordId.IsEmpty() && !Records.IsEmpty())
    {
        SelectedRecordId = Records.Last().RecordId;
    }
    Comparison = LoadedComparison;
    OutError.Reset();
    return true;
}
