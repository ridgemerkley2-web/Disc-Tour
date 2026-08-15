#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"
#include "Math/RotationMatrix.h"

struct FDiscReplayFrame
{
    FVector WorldLocationCm = FVector::ZeroVector;
    FVector VelocityMps = FVector::ZeroVector;
    FVector DiscNormalWorld = FVector::UpVector;
    FQuat WorldRotation = FQuat::Identity;
    float SpinRpm = 0.0f;
    float AngleOfAttackDeg = 0.0f;
    EDiscGroundState GroundState = EDiscGroundState::Airborne;
    int32 GroundContactCount = 0;
    int32 LowerSampleIndex = 0;
    float SegmentAlpha = 0.0f;
};

namespace DiscGolfPresentationMath
{
    inline FQuat ReplayRotation(const FVector& VelocityMps, const FVector& DiscNormalWorld)
    {
        const FVector Normal = DiscNormalWorld.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
        FVector Forward = VelocityMps - Normal * FVector::DotProduct(VelocityMps, Normal);
        if (!Forward.Normalize())
        {
            Forward = FVector::ForwardVector - Normal * FVector::DotProduct(FVector::ForwardVector, Normal);
            if (!Forward.Normalize())
            {
                Forward = FVector::RightVector - Normal * FVector::DotProduct(FVector::RightVector, Normal);
                Forward.Normalize();
            }
        }
        return FRotationMatrix::MakeFromXZ(Forward, Normal).ToQuat();
    }

    inline bool EvaluateReplayFrame(
        const TArray<FDiscTrajectorySample>& Samples,
        float PlaybackTimeSeconds,
        FDiscReplayFrame& OutFrame)
    {
        if (Samples.IsEmpty()) return false;

        const float FirstTime = Samples[0].TimeSeconds;
        const float TargetTime = FirstTime + FMath::Max(PlaybackTimeSeconds, 0.0f);
        int32 LowerIndex = 0;
        int32 UpperIndex = 0;

        if (TargetTime >= Samples.Last().TimeSeconds)
        {
            LowerIndex = Samples.Num() - 1;
            UpperIndex = LowerIndex;
        }
        else if (TargetTime > FirstTime)
        {
            int32 Low = 1;
            int32 High = Samples.Num() - 1;
            while (Low < High)
            {
                const int32 Mid = Low + (High - Low) / 2;
                if (Samples[Mid].TimeSeconds < TargetTime) Low = Mid + 1;
                else High = Mid;
            }
            UpperIndex = Low;
            LowerIndex = FMath::Max(UpperIndex - 1, 0);
        }

        const FDiscTrajectorySample& A = Samples[LowerIndex];
        const FDiscTrajectorySample& B = Samples[UpperIndex];
        const float SegmentSeconds = B.TimeSeconds - A.TimeSeconds;
        const float Alpha = LowerIndex == UpperIndex || SegmentSeconds <= SMALL_NUMBER
            ? 0.0f
            : FMath::Clamp((TargetTime - A.TimeSeconds) / SegmentSeconds, 0.0f, 1.0f);

        OutFrame.WorldLocationCm = FMath::Lerp(A.WorldLocationCm, B.WorldLocationCm, Alpha);
        OutFrame.VelocityMps = FMath::Lerp(A.VelocityMps, B.VelocityMps, Alpha);
        OutFrame.DiscNormalWorld = FMath::Lerp(A.DiscNormalWorld, B.DiscNormalWorld, Alpha)
            .GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
        OutFrame.WorldRotation = ReplayRotation(OutFrame.VelocityMps, OutFrame.DiscNormalWorld);
        OutFrame.SpinRpm = FMath::Lerp(A.SpinRpm, B.SpinRpm, Alpha);
        OutFrame.AngleOfAttackDeg = FMath::Lerp(A.AngleOfAttackDeg, B.AngleOfAttackDeg, Alpha);
        const FDiscTrajectorySample& StateSample = Alpha < 0.5f ? A : B;
        OutFrame.GroundState = StateSample.GroundState;
        OutFrame.GroundContactCount = StateSample.GroundContactCount;
        OutFrame.LowerSampleIndex = LowerIndex;
        OutFrame.SegmentAlpha = Alpha;
        return true;
    }

    inline void BuildTracerSampleIndices(
        const TArray<FDiscTrajectorySample>& Samples,
        TArray<int32>& OutIndices,
        float MinimumDistanceCm = 75.0f,
        float MaximumTimeGapSeconds = 0.075f,
        int32 MaximumPoints = 320)
    {
        OutIndices.Reset();
        if (Samples.IsEmpty() || MaximumPoints <= 0) return;
        OutIndices.Add(0);
        if (Samples.Num() == 1 || MaximumPoints == 1) return;

        const float SafeDistanceSq = FMath::Square(FMath::Max(MinimumDistanceCm, 1.0f));
        const float SafeTimeGap = FMath::Max(MaximumTimeGapSeconds, 1.0f / 240.0f);
        int32 LastAccepted = 0;
        for (int32 Index = 1; Index < Samples.Num() - 1 && OutIndices.Num() < MaximumPoints - 1; ++Index)
        {
            const FDiscTrajectorySample& Sample = Samples[Index];
            const FDiscTrajectorySample& Last = Samples[LastAccepted];
            const FDiscTrajectorySample& Previous = Samples[Index - 1];
            const bool bStateBoundary = Sample.GroundState != Previous.GroundState
                || Sample.GroundContactCount != Previous.GroundContactCount;
            const bool bFarEnough = FVector::DistSquared(Sample.WorldLocationCm, Last.WorldLocationCm) >= SafeDistanceSq;
            const bool bOldEnough = Sample.TimeSeconds - Last.TimeSeconds >= SafeTimeGap;
            if (bStateBoundary || bFarEnough || bOldEnough)
            {
                OutIndices.Add(Index);
                LastAccepted = Index;
            }
        }

        const int32 FinalIndex = Samples.Num() - 1;
        if (OutIndices.Last() != FinalIndex)
        {
            OutIndices.Add(FinalIndex);
        }
    }
}
