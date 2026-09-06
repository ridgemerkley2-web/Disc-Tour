#include "DiscGolfTerrainPresentationMath.h"

namespace
{
bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}
}

bool DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
    const TArray<FVector>& SourcePoints,
    const FVector& GroundStartCm,
    const FVector& GroundEndCm,
    TArray<FVector>& OutNormalizedPoints)
{
    if (SourcePoints.Num() < 2
        || !IsFiniteVector(GroundStartCm)
        || !IsFiniteVector(GroundEndCm))
    {
        return false;
    }
    for (const FVector& Point : SourcePoints)
    {
        if (!IsFiniteVector(Point)) return false;
    }

    TArray<FVector> Candidate = SourcePoints;
    const float StartClearanceCm = Candidate[0].Z - GroundStartCm.Z;
    const float EndClearanceCm = Candidate.Last().Z - GroundEndCm.Z;
    if (!FMath::IsFinite(StartClearanceCm) || !FMath::IsFinite(EndClearanceCm))
    {
        return false;
    }

    Candidate[0].X = GroundStartCm.X;
    Candidate[0].Y = GroundStartCm.Y;
    Candidate.Last().X = GroundEndCm.X;
    Candidate.Last().Y = GroundEndCm.Y;

    TArray<float> CumulativeDistancesCm;
    CumulativeDistancesCm.Init(0.0f, Candidate.Num());
    for (int32 Index = 1; Index < Candidate.Num(); ++Index)
    {
        const float SegmentDistanceCm = FVector2D::Distance(
            FVector2D(Candidate[Index - 1]), FVector2D(Candidate[Index]));
        if (!FMath::IsFinite(SegmentDistanceCm)) return false;

        CumulativeDistancesCm[Index] = CumulativeDistancesCm[Index - 1]
            + SegmentDistanceCm;
        if (!FMath::IsFinite(CumulativeDistancesCm[Index])) return false;
    }

    const float TotalDistanceCm = CumulativeDistancesCm.Last();
    if (TotalDistanceCm <= KINDA_SMALL_NUMBER) return false;

    for (int32 Index = 0; Index < Candidate.Num(); ++Index)
    {
        const float AlongPath01 = CumulativeDistancesCm[Index] / TotalDistanceCm;
        Candidate[Index].Z -= FMath::Lerp(
            StartClearanceCm, EndClearanceCm, AlongPath01);
        if (!IsFiniteVector(Candidate[Index])) return false;
    }

    Candidate[0] = GroundStartCm;
    Candidate.Last() = GroundEndCm;
    OutNormalizedPoints = MoveTemp(Candidate);
    return true;
}
