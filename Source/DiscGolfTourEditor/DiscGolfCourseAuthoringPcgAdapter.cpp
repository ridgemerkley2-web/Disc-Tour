#include "DiscGolfCourseAuthoringPcgAdapter.h"

namespace
{
    bool IsFinitePoint(const FVector& Point)
    {
        return FMath::IsFinite(Point.X)
            && FMath::IsFinite(Point.Y)
            && FMath::IsFinite(Point.Z);
    }

    double TwiceSignedArea2D(const TArray<FVector>& Points)
    {
        double TwiceSignedArea = 0.0;
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            const FVector& A = Points[Index];
            const FVector& B = Points[(Index + 1) % Points.Num()];
            TwiceSignedArea += static_cast<double>(A.X) * static_cast<double>(B.Y)
                - static_cast<double>(B.X) * static_cast<double>(A.Y);
        }
        return TwiceSignedArea;
    }

    double Orientation2D(const FVector& A, const FVector& B, const FVector& C)
    {
        return (static_cast<double>(B.X) - A.X) * (static_cast<double>(C.Y) - A.Y)
            - (static_cast<double>(B.Y) - A.Y) * (static_cast<double>(C.X) - A.X);
    }

    bool IsPointOnSegment2D(const FVector& Point, const FVector& A, const FVector& B)
    {
        constexpr double Tolerance = 1.0e-6;
        return FMath::Abs(Orientation2D(A, B, Point)) <= Tolerance
            && static_cast<double>(Point.X) >= FMath::Min<double>(A.X, B.X) - Tolerance
            && static_cast<double>(Point.X) <= FMath::Max<double>(A.X, B.X) + Tolerance
            && static_cast<double>(Point.Y) >= FMath::Min<double>(A.Y, B.Y) - Tolerance
            && static_cast<double>(Point.Y) <= FMath::Max<double>(A.Y, B.Y) + Tolerance;
    }

    bool SegmentsIntersect2D(
        const FVector& A,
        const FVector& B,
        const FVector& C,
        const FVector& D)
    {
        constexpr double Tolerance = 1.0e-6;
        const double ABC = Orientation2D(A, B, C);
        const double ABD = Orientation2D(A, B, D);
        const double CDA = Orientation2D(C, D, A);
        const double CDB = Orientation2D(C, D, B);
        const bool bProperIntersection = ((ABC > Tolerance && ABD < -Tolerance)
                || (ABC < -Tolerance && ABD > Tolerance))
            && ((CDA > Tolerance && CDB < -Tolerance)
                || (CDA < -Tolerance && CDB > Tolerance));
        return bProperIntersection
            || IsPointOnSegment2D(C, A, B)
            || IsPointOnSegment2D(D, A, B)
            || IsPointOnSegment2D(A, C, D)
            || IsPointOnSegment2D(B, C, D);
    }

    bool HasSelfIntersection2D(const TArray<FVector>& Points)
    {
        const int32 PointCount = Points.Num();
        for (int32 FirstIndex = 0; FirstIndex < PointCount; ++FirstIndex)
        {
            const int32 FirstNext = (FirstIndex + 1) % PointCount;
            for (int32 SecondIndex = FirstIndex + 1; SecondIndex < PointCount; ++SecondIndex)
            {
                const int32 SecondNext = (SecondIndex + 1) % PointCount;
                if (FirstNext == SecondIndex || SecondNext == FirstIndex)
                {
                    continue;
                }
                if (SegmentsIntersect2D(
                    Points[FirstIndex], Points[FirstNext],
                    Points[SecondIndex], Points[SecondNext]))
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool IsHardClearanceType(EDGCourseZoneType ZoneType)
    {
        return ZoneType == EDGCourseZoneType::TeeSafety
            || ZoneType == EDGCourseZoneType::Green
            || ZoneType == EDGCourseZoneType::WaterHazard
            || ZoneType == EDGCourseZoneType::Spectator
            || ZoneType == EDGCourseZoneType::NoSpawn;
    }
}

bool DiscGolfCourseAuthoringPcgAdapter::TryGetZonePolicy(
    EDGCourseZoneType SourceZoneType,
    FDiscGolfCourseAuthoringPcgZonePolicy& OutPolicy,
    FString& OutError)
{
    FDiscGolfCourseAuthoringPcgZonePolicy Candidate;
    switch (SourceZoneType)
    {
        case EDGCourseZoneType::TeeSafety:
            Candidate = {EDiscGolfEnvironmentZoneType::Tee, 100, true, false};
            break;
        case EDGCourseZoneType::FairwayPrimary:
            Candidate = {EDiscGolfEnvironmentZoneType::Fairway, 50, false, true};
            break;
        case EDGCourseZoneType::FairwaySecondary:
            Candidate = {EDiscGolfEnvironmentZoneType::Fairway, 45, false, true};
            break;
        case EDGCourseZoneType::Rough:
            Candidate = {EDiscGolfEnvironmentZoneType::SemiRough, 30, false, false};
            break;
        case EDGCourseZoneType::DeepRough:
            Candidate = {EDiscGolfEnvironmentZoneType::DeepRough, 10, false, false};
            break;
        case EDGCourseZoneType::Green:
            Candidate = {EDiscGolfEnvironmentZoneType::Green, 100, true, false};
            break;
        case EDGCourseZoneType::OutOfBounds:
            Candidate = {EDiscGolfEnvironmentZoneType::OBNatural, 20, false, false};
            break;
        case EDGCourseZoneType::WaterHazard:
            Candidate = {EDiscGolfEnvironmentZoneType::OBNatural, 110, true, false};
            break;
        case EDGCourseZoneType::Spectator:
            Candidate = {EDiscGolfEnvironmentZoneType::OBNatural, 120, true, false};
            break;
        case EDGCourseZoneType::NoSpawn:
            Candidate = {EDiscGolfEnvironmentZoneType::OBNatural, 130, true, false};
            break;
        default:
            OutPolicy = {};
            OutError = FString::Printf(TEXT("unsupported course zone type value %d"),
                static_cast<int32>(SourceZoneType));
            return false;
    }

    OutPolicy = Candidate;
    OutError.Reset();
    return true;
}

bool DiscGolfCourseAuthoringPcgAdapter::BuildPlan(
    const TArray<FDGCourseZoneDefinition>& AuthoritativeGameplayZones,
    TArray<FDiscGolfCourseAuthoringPcgZonePlanEntry>& OutPlan,
    FString& OutError)
{
    OutPlan.Reset();
    OutError.Reset();

    TSet<FName> StableIds;
    TArray<FDiscGolfCourseAuthoringPcgZonePlanEntry> CandidatePlan;
    CandidatePlan.Reserve(AuthoritativeGameplayZones.Num());

    for (const FDGCourseZoneDefinition& Zone : AuthoritativeGameplayZones)
    {
        if (Zone.ZoneId.IsNone())
        {
            OutError = TEXT("every PCG source zone requires an explicit stable ID");
            return false;
        }
        if (StableIds.Contains(Zone.ZoneId))
        {
            OutError = FString::Printf(TEXT("duplicate PCG source zone ID '%s'"),
                *Zone.ZoneId.ToString());
            return false;
        }
        StableIds.Add(Zone.ZoneId);

        FDiscGolfCourseAuthoringPcgZonePolicy Policy;
        if (!TryGetZonePolicy(Zone.ZoneType, Policy, OutError))
        {
            return false;
        }

        // Gameplay penalties are not copied into the decorative plan, but malformed source
        // authority must not become acceptable merely because this seam does not consume it.
        if (Zone.PenaltyStrokes < 0)
        {
            OutError = FString::Printf(
                TEXT("zone '%s' has a negative gameplay penalty"),
                *Zone.ZoneId.ToString());
            return false;
        }

        if (Zone.PolygonPointsCm.Num() < 3 || Zone.PolygonPointsCm.Num() > 256)
        {
            OutError = FString::Printf(
                TEXT("zone '%s' requires 3-256 explicit polygon points for the PCG seam"),
                *Zone.ZoneId.ToString());
            return false;
        }
        for (int32 PointIndex = 0; PointIndex < Zone.PolygonPointsCm.Num(); ++PointIndex)
        {
            if (!IsFinitePoint(Zone.PolygonPointsCm[PointIndex]))
            {
                OutError = FString::Printf(TEXT("zone '%s' point %d is non-finite"),
                    *Zone.ZoneId.ToString(), PointIndex);
                return false;
            }
            const int32 NextIndex = (PointIndex + 1) % Zone.PolygonPointsCm.Num();
            if (Zone.PolygonPointsCm[PointIndex].Equals(
                Zone.PolygonPointsCm[NextIndex], UE_KINDA_SMALL_NUMBER))
            {
                OutError = FString::Printf(
                    TEXT("zone '%s' has ambiguous consecutive/closing duplicate points"),
                    *Zone.ZoneId.ToString());
                return false;
            }
        }
        // Point count is capped above, so this deterministic O(n^2) crossing check has a fixed
        // maximum of 32,640 segment pairs and cannot turn editor export into an unbounded pass.
        if (HasSelfIntersection2D(Zone.PolygonPointsCm))
        {
            OutError = FString::Printf(TEXT("zone '%s' polygon self-intersects"),
                *Zone.ZoneId.ToString());
            return false;
        }
        constexpr double MinimumPolygonAreaSquareCm = 10000.0;
        if (FMath::Abs(TwiceSignedArea2D(Zone.PolygonPointsCm)) * 0.5
            < MinimumPolygonAreaSquareCm)
        {
            OutError = FString::Printf(
                TEXT("zone '%s' polygon area is below %.0f square centimeters"),
                *Zone.ZoneId.ToString(), MinimumPolygonAreaSquareCm);
            return false;
        }

        // Safety clearances cannot opt out of vegetation handling: doing so would make quality or
        // downstream defaults capable of repopulating a gameplay-critical clearance footprint.
        if (IsHardClearanceType(Zone.ZoneType) && !Zone.bAffectsVegetation)
        {
            OutError = FString::Printf(
                TEXT("zone '%s' is a hard clearance but vegetation influence is disabled"),
                *Zone.ZoneId.ToString());
            return false;
        }

        FDiscGolfCourseAuthoringPcgZonePlanEntry Entry;
        Entry.StableZoneId = Zone.ZoneId;
        Entry.SourceZoneType = Zone.ZoneType;
        Entry.EnvironmentZoneType = Policy.EnvironmentZoneType;
        Entry.PolygonPointsCm = Zone.PolygonPointsCm;
        Entry.Priority = Policy.Priority;
        Entry.bApplyToDecorativePcg = Zone.bAffectsVegetation;
        Entry.bHardExclusion = Policy.bHardExclusion;
        Entry.bExcludeTreesFromFlightLine = Policy.bExcludeTreesFromFlightLine;
        CandidatePlan.Add(MoveTemp(Entry));
    }

    CandidatePlan.Sort([](const FDiscGolfCourseAuthoringPcgZonePlanEntry& A,
        const FDiscGolfCourseAuthoringPcgZonePlanEntry& B)
    {
        return A.StableZoneId.LexicalLess(B.StableZoneId);
    });

    OutPlan = MoveTemp(CandidatePlan);
    return true;
}
