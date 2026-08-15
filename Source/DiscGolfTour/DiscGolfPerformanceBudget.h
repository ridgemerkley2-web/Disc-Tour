#pragma once

#include "CoreMinimal.h"

enum class EDiscGolfPerformanceBudgetState : uint8
{
    WarmingUp,
    Pass,
    Warning,
    Fail
};

struct FDiscGolfPerformanceBudget
{
    float TargetFrameTimeMs = 16.67f;
    float WarningP95FrameTimeMs = 22.0f;
    float FailP95FrameTimeMs = 33.34f;
    float HitchFrameTimeMs = 50.0f;
    float MaxHitchRatePercent = 1.0f;
    uint64 WarningUsedPhysicalBytes = 3758096384ull; // 3.5 GiB
    uint64 MaxUsedPhysicalBytes = 4294967296ull; // 4.0 GiB
    int32 MinimumSampleCount = 120;
    int32 WindowSampleCount = 600;
};

struct FDiscGolfPerformanceSummary
{
    int32 SampleCount = 0;
    float AverageFrameTimeMs = 0.0f;
    float P95FrameTimeMs = 0.0f;
    float MaxFrameTimeMs = 0.0f;
    float AverageFps = 0.0f;
    int32 HitchCount = 0;
    float HitchRatePercent = 0.0f;
    uint64 UsedPhysicalBytes = 0;
    bool bHasMinimumSamples = false;
};

/** Bounded presentation telemetry. It observes render-frame timing and never feeds gameplay or simulation. */
class FDiscGolfPerformanceTracker
{
public:
    explicit FDiscGolfPerformanceTracker(const FDiscGolfPerformanceBudget& InBudget = FDiscGolfPerformanceBudget())
        : Budget(InBudget)
    {
        FrameTimesMs.SetNumZeroed(FMath::Max(Budget.WindowSampleCount, 1));
    }

    void Reset()
    {
        for (float& FrameTime : FrameTimesMs) FrameTime = 0.0f;
        NextSampleIndex = 0;
        SampleCount = 0;
    }

    void AddFrame(float DeltaSeconds)
    {
        if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f || FrameTimesMs.IsEmpty()) return;
        FrameTimesMs[NextSampleIndex] = FMath::Clamp(DeltaSeconds * 1000.0f, 0.01f, 1000.0f);
        NextSampleIndex = (NextSampleIndex + 1) % FrameTimesMs.Num();
        SampleCount = FMath::Min(SampleCount + 1, FrameTimesMs.Num());
    }

    FDiscGolfPerformanceSummary Summarize(uint64 UsedPhysicalBytes) const
    {
        FDiscGolfPerformanceSummary Summary;
        Summary.SampleCount = SampleCount;
        Summary.UsedPhysicalBytes = UsedPhysicalBytes;
        Summary.bHasMinimumSamples = SampleCount >= Budget.MinimumSampleCount;
        if (SampleCount <= 0) return Summary;

        TArray<float> SortedFrameTimes;
        SortedFrameTimes.Reserve(SampleCount);
        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            const float FrameTimeMs = FrameTimesMs[Index];
            SortedFrameTimes.Add(FrameTimeMs);
            Summary.AverageFrameTimeMs += FrameTimeMs;
            Summary.MaxFrameTimeMs = FMath::Max(Summary.MaxFrameTimeMs, FrameTimeMs);
            if (FrameTimeMs >= Budget.HitchFrameTimeMs) ++Summary.HitchCount;
        }
        Summary.AverageFrameTimeMs /= static_cast<float>(SampleCount);
        Summary.AverageFps = Summary.AverageFrameTimeMs > SMALL_NUMBER
            ? 1000.0f / Summary.AverageFrameTimeMs : 0.0f;
        Summary.HitchRatePercent = 100.0f * Summary.HitchCount / static_cast<float>(SampleCount);
        SortedFrameTimes.Sort();
        const int32 P95Index = FMath::Clamp(FMath::CeilToInt(SampleCount * 0.95f) - 1, 0, SampleCount - 1);
        Summary.P95FrameTimeMs = SortedFrameTimes[P95Index];
        return Summary;
    }

    const FDiscGolfPerformanceBudget& GetBudget() const { return Budget; }

private:
    FDiscGolfPerformanceBudget Budget;
    TArray<float> FrameTimesMs;
    int32 NextSampleIndex = 0;
    int32 SampleCount = 0;
};

namespace DiscGolfPerformance
{
    inline EDiscGolfPerformanceBudgetState Evaluate(
        const FDiscGolfPerformanceSummary& Summary,
        const FDiscGolfPerformanceBudget& Budget)
    {
        if (!Summary.bHasMinimumSamples) return EDiscGolfPerformanceBudgetState::WarmingUp;
        if (Summary.P95FrameTimeMs > Budget.FailP95FrameTimeMs
            || Summary.HitchRatePercent > Budget.MaxHitchRatePercent
            || Summary.UsedPhysicalBytes > Budget.MaxUsedPhysicalBytes)
        {
            return EDiscGolfPerformanceBudgetState::Fail;
        }
        if (Summary.P95FrameTimeMs > Budget.WarningP95FrameTimeMs
            || Summary.HitchCount > 0
            || Summary.UsedPhysicalBytes > Budget.WarningUsedPhysicalBytes)
        {
            return EDiscGolfPerformanceBudgetState::Warning;
        }
        return EDiscGolfPerformanceBudgetState::Pass;
    }

    inline FString StateName(EDiscGolfPerformanceBudgetState State)
    {
        switch (State)
        {
            case EDiscGolfPerformanceBudgetState::Pass: return TEXT("PASS");
            case EDiscGolfPerformanceBudgetState::Warning: return TEXT("WARN");
            case EDiscGolfPerformanceBudgetState::Fail: return TEXT("FAIL");
            default: return TEXT("WARMING");
        }
    }

    inline FString StatusText(
        const FDiscGolfPerformanceSummary& Summary,
        const FDiscGolfPerformanceBudget& Budget)
    {
        const EDiscGolfPerformanceBudgetState State = Evaluate(Summary, Budget);
        if (State == EDiscGolfPerformanceBudgetState::WarmingUp)
        {
            return FString::Printf(TEXT("PERF WARMING %d/%d"),
                Summary.SampleCount, Budget.MinimumSampleCount);
        }
        return FString::Printf(TEXT("PERF %s // P95 %.1f MS // %.0f FPS // %.1f GB"),
            *StateName(State), Summary.P95FrameTimeMs, Summary.AverageFps,
            Summary.UsedPhysicalBytes / 1073741824.0);
    }
}
