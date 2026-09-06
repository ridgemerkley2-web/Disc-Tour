#include "DiscGolfPresentationMath.h"

namespace
{
bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

template <typename T>
bool IsValidEnum(T Value)
{
    const UEnum* Enum = StaticEnum<T>();
    return Enum && Enum->IsValidEnumValue(static_cast<int64>(Value));
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

bool IsValidPolicy(const FDiscActualReplaySelectionPolicy& Policy)
{
    return Policy.MaxSourceSamples >= 2
        && Policy.MaxOutputSamples >= 2
        && Policy.MaxOutputSamples <= Policy.MaxSourceSamples
        && FMath::IsFinite(Policy.MaxSampleRateHz)
        && Policy.MaxSampleRateHz > 0.0f
        && FMath::IsFinite(Policy.MaxDurationSeconds)
        && Policy.MaxDurationSeconds > 0.0f;
}
}

bool DiscGolfPresentationMath::ValidateActualReplaySource(
    const TArray<FDiscTrajectorySample>& Source,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscActualReplaySelectionPolicy& Policy,
    FString& OutError)
{
    if (!IsValidPolicy(Policy))
    {
        OutError = TEXT("Actual replay selection policy is invalid");
        return false;
    }
    if (Source.Num() < 2 || Source.Num() > Policy.MaxSourceSamples)
    {
        OutError = FString::Printf(TEXT("Replay source must contain 2..%d actual samples"),
            Policy.MaxSourceSamples);
        return false;
    }

    float PreviousTime = -1.0f;
    for (int32 Index = 0; Index < Source.Num(); ++Index)
    {
        const FDiscTrajectorySample& Sample = Source[Index];
        if (!IsFiniteSample(Sample))
        {
            OutError = TEXT("Replay source samples must be finite and valid");
            return false;
        }
        if (Index > 0 && Sample.TimeSeconds < PreviousTime)
        {
            OutError = TEXT("Replay source samples must be time ordered");
            return false;
        }
        if (Index > 0 && Sample.TimeSeconds == PreviousTime
            && !Sample.WorldLocationCm.Equals(Source[Index - 1].WorldLocationCm, KINDA_SMALL_NUMBER))
        {
            OutError = TEXT("Same-time replay source state changes must share one actual world position");
            return false;
        }
        PreviousTime = Sample.TimeSeconds;
    }

    const float Duration = Source.Last().TimeSeconds - Source[0].TimeSeconds;
    if (!FMath::IsFinite(Duration) || Duration <= SMALL_NUMBER
        || Duration > Policy.MaxDurationSeconds)
    {
        OutError = TEXT("Replay source duration is outside the bounded capture window");
        return false;
    }

    float PreviousTransitionTime = -1.0f;
    for (const FDiscGroundTransition& Transition : Transitions)
    {
        if (!IsFiniteTransition(Transition)
            || Transition.TimeSeconds + KINDA_SMALL_NUMBER < PreviousTransitionTime
            || Transition.TimeSeconds + KINDA_SMALL_NUMBER < Source[0].TimeSeconds
            || Transition.TimeSeconds > Source.Last().TimeSeconds + KINDA_SMALL_NUMBER)
        {
            OutError = TEXT("Replay transitions must be finite, ordered, and inside the actual sample window");
            return false;
        }
        PreviousTransitionTime = Transition.TimeSeconds;
    }

    OutError.Reset();
    return true;
}

bool DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
    const TArray<FDiscTrajectorySample>& Source,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscActualReplaySelectionPolicy& Policy,
    TArray<FDiscTrajectorySample>& OutSamples,
    FString& OutError)
{
    OutSamples.Reset();
    if (!ValidateActualReplaySource(Source, Transitions, Policy, OutError))
    {
        return false;
    }

    // The solver deliberately appends its terminal Settled state at the same
    // timestamp and position as the final moving sample. Retain the later actual
    // state from every such cluster so replay actor input stays strictly ordered.
    TArray<int32> CanonicalSourceIndices;
    CanonicalSourceIndices.Reserve(Source.Num());
    for (int32 SourceIndex = 0; SourceIndex < Source.Num(); ++SourceIndex)
    {
        if (!CanonicalSourceIndices.IsEmpty()
            && Source[SourceIndex].TimeSeconds
                == Source[CanonicalSourceIndices.Last()].TimeSeconds)
        {
            CanonicalSourceIndices.Last() = SourceIndex;
        }
        else
        {
            CanonicalSourceIndices.Add(SourceIndex);
        }
    }
    if (CanonicalSourceIndices.Num() < 2)
    {
        OutError = TEXT("Replay source must contain two distinct actual sample times");
        return false;
    }

    const float Duration = Source.Last().TimeSeconds - Source[0].TimeSeconds;
    const int32 RateBudget =
        FMath::FloorToInt(Duration * Policy.MaxSampleRateHz + KINDA_SMALL_NUMBER) + 1;
    if (RateBudget < 2)
    {
        OutError = TEXT("Replay source duration is too short for the configured sample-rate bound");
        return false;
    }
    const int32 SampleBudget = FMath::Min(Policy.MaxOutputSamples, RateBudget);

    TSet<int32> RequiredIndices;
    RequiredIndices.Add(0);
    RequiredIndices.Add(CanonicalSourceIndices.Num() - 1);
    for (int32 Index = 1; Index < CanonicalSourceIndices.Num(); ++Index)
    {
        const FDiscTrajectorySample& Previous = Source[CanonicalSourceIndices[Index - 1]];
        const FDiscTrajectorySample& Current = Source[CanonicalSourceIndices[Index]];
        if (Previous.GroundState != Current.GroundState
            || Previous.GroundContactCount != Current.GroundContactCount
            || Previous.GroundSurface != Current.GroundSurface
            || Previous.CourseSurface != Current.CourseSurface)
        {
            RequiredIndices.Add(Index);
        }
    }

    int32 SearchIndex = 0;
    for (const FDiscGroundTransition& Transition : Transitions)
    {
        while (SearchIndex + 1 < CanonicalSourceIndices.Num()
            && Source[CanonicalSourceIndices[SearchIndex + 1]].TimeSeconds <= Transition.TimeSeconds)
        {
            ++SearchIndex;
        }
        int32 ClosestIndex = SearchIndex;
        if (SearchIndex + 1 < CanonicalSourceIndices.Num()
            && FMath::Abs(Source[CanonicalSourceIndices[SearchIndex + 1]].TimeSeconds - Transition.TimeSeconds)
                < FMath::Abs(Source[CanonicalSourceIndices[SearchIndex]].TimeSeconds - Transition.TimeSeconds))
        {
            ClosestIndex = SearchIndex + 1;
        }
        RequiredIndices.Add(ClosestIndex);
    }

    if (RequiredIndices.Num() > SampleBudget)
    {
        OutError = FString::Printf(
            TEXT("%d discrete replay boundaries exceed the %d-sample %.0f Hz budget"),
            RequiredIndices.Num(), SampleBudget, Policy.MaxSampleRateHz);
        return false;
    }

    const int32 RemainingBudget = SampleBudget - RequiredIndices.Num();
    for (int32 Slot = 1; Slot <= RemainingBudget; ++Slot)
    {
        const int32 Index = FMath::RoundToInt(
            static_cast<double>(Slot) * static_cast<double>(CanonicalSourceIndices.Num() - 1)
            / static_cast<double>(RemainingBudget + 1));
        RequiredIndices.Add(FMath::Clamp(Index, 1, CanonicalSourceIndices.Num() - 2));
    }

    TArray<int32> SelectedIndices = RequiredIndices.Array();
    SelectedIndices.Sort();
    OutSamples.Reserve(SelectedIndices.Num());
    for (const int32 Index : SelectedIndices)
    {
        // The selector only copies source indices; playback may interpolate a
        // visual frame later, but it never invents or resimulates trajectory data.
        OutSamples.Add(Source[CanonicalSourceIndices[Index]]);
    }
    OutError.Reset();
    return true;
}
