#include "PineRidgeHole1Environment.h"

#include "DiscGolfCourseDefinition.h"

namespace
{
    float SegmentDistance2D(const FVector& Point, const FVector& A, const FVector& B)
    {
        const FVector2D P(Point.X, Point.Y);
        const FVector2D Start(A.X, A.Y);
        const FVector2D End(B.X, B.Y);
        const FVector2D Segment = End - Start;
        const float LengthSquared = Segment.SizeSquared();
        if (LengthSquared <= KINDA_SMALL_NUMBER) return FVector2D::Distance(P, Start);
        const float Alpha = FMath::Clamp(FVector2D::DotProduct(P - Start, Segment) / LengthSquared, 0.0f, 1.0f);
        return FVector2D::Distance(P, Start + Segment * Alpha);
    }

    float RouteDistance2D(const FVector& Point, const TArray<FVector>& Route)
    {
        float Best = TNumericLimits<float>::Max();
        for (int32 Index = 1; Index < Route.Num(); ++Index)
        {
            Best = FMath::Min(Best, SegmentDistance2D(Point, Route[Index - 1], Route[Index]));
        }
        return Best;
    }

    TArray<FVector> RoutePoints(
        const FDiscGolfHoleBlockoutDefinition& Definition,
        EDiscGolfShotRouteType RouteType)
    {
        if (const FDiscGolfShotRouteDefinition* Route = Definition.ShotRoutes.FindByPredicate(
            [RouteType](const FDiscGolfShotRouteDefinition& Candidate)
            {
                return Candidate.RouteType == RouteType;
            }))
        {
            return Route->WaypointsCm;
        }
        return {};
    }

    FDiscGolfHole1EnvironmentZonePlan SplineZone(
        FName Id,
        EDiscGolfEnvironmentZoneType Type,
        int32 Priority,
        float WidthCm,
        float TreeSetbackCm,
        float BrushSetbackCm,
        float BlendFalloffCm,
        TArray<FVector> Points)
    {
        FDiscGolfHole1EnvironmentZonePlan Zone;
        Zone.ZoneId = Id;
        Zone.ZoneType = Type;
        Zone.Shape = EDiscGolfEnvironmentZoneShape::SplineCorridor;
        Zone.Priority = Priority;
        Zone.WidthCm = WidthCm;
        Zone.TreeSetbackCm = TreeSetbackCm;
        Zone.BrushSetbackCm = BrushSetbackCm;
        Zone.BlendFalloffCm = BlendFalloffCm;
        Zone.SplinePointsCm = MoveTemp(Points);
        return Zone;
    }

    FDiscGolfHole1EnvironmentZonePlan RadialZone(
        FName Id,
        EDiscGolfEnvironmentZoneType Type,
        int32 Priority,
        float RadiusCm,
        float BlendFalloffCm,
        const FVector& Location)
    {
        FDiscGolfHole1EnvironmentZonePlan Zone;
        Zone.ZoneId = Id;
        Zone.ZoneType = Type;
        Zone.Shape = EDiscGolfEnvironmentZoneShape::Radial;
        Zone.Priority = Priority;
        Zone.bHardExclusion = true;
        Zone.RadiusCm = RadiusCm;
        Zone.BlendFalloffCm = BlendFalloffCm;
        Zone.LocationCm = Location;
        return Zone;
    }
}

TArray<FDiscGolfHole1EnvironmentZonePlan> PineRidgeHole1Environment::BuildZonePlan(
    const FDiscGolfHoleBlockoutDefinition& Definition)
{
    TArray<FDiscGolfHole1EnvironmentZonePlan> Result;
    if (Definition.HoleNumber != 1) return Result;

    const TArray<FVector> Primary = RoutePoints(Definition, EDiscGolfShotRouteType::Primary);
    const TArray<FVector> Alternate = RoutePoints(Definition, EDiscGolfShotRouteType::Bailout);

    FDiscGolfHole1EnvironmentZonePlan DeepRough;
    DeepRough.ZoneId = TEXT("H01_DeepRoughEcology");
    DeepRough.ZoneType = EDiscGolfEnvironmentZoneType::DeepRough;
    DeepRough.Shape = EDiscGolfEnvironmentZoneShape::Box;
    DeepRough.Priority = 10;
    DeepRough.LocationCm = (Definition.TeeLocationCm + Definition.BasketLocationCm) * 0.5f;
    DeepRough.BoxExtentCm = FVector(7600.0f, 6200.0f, 3000.0f);
    DeepRough.BlendFalloffCm = 500.0f;
    Result.Add(DeepRough);

    Result.Add(SplineZone(TEXT("H01_SemiRoughTransition"),
        EDiscGolfEnvironmentZoneType::SemiRough, 30, 4500.0f, 0.0f, 0.0f, 900.0f, Primary));
    Result.Add(SplineZone(TEXT("H01_PrimaryFairway"),
        EDiscGolfEnvironmentZoneType::Fairway, 50, 1680.0f, 610.0f, 275.0f, 650.0f, Primary));
    Result.Add(SplineZone(TEXT("H01_TurnoverFairway"),
        EDiscGolfEnvironmentZoneType::Fairway, 45, 1370.0f, 480.0f, 225.0f, 550.0f, Alternate));
    Result.Add(RadialZone(TEXT("H01_TeeClear"), EDiscGolfEnvironmentZoneType::Tee,
        100, 1067.0f, 300.0f, Definition.TeeLocationCm));
    Result.Add(RadialZone(TEXT("H01_GreenClear"), EDiscGolfEnvironmentZoneType::Green,
        100, 1067.0f, 350.0f, Definition.BasketLocationCm));
    return Result;
}

bool PineRidgeHole1Environment::ValidateClearance(
    const FDiscGolfHoleBlockoutDefinition& Definition,
    const TArray<FDiscGolfHole1EnvironmentZonePlan>& Zones,
    FString& OutError)
{
    if (Definition.HoleNumber != 1 || Definition.Par != 3)
    {
        OutError = TEXT("benchmark environment must belong to Pine Ridge Hole 1, par 3");
        return false;
    }
    const float Feet = DiscGolfCourseDefinition::MeasuredDistanceFeet(Definition);
    if (Feet < 340.0f || Feet > 390.0f)
    {
        OutError = FString::Printf(TEXT("Hole 1 distance %.1f ft is outside the 340-390 ft target"), Feet);
        return false;
    }

    const FDiscGolfHole1EnvironmentZonePlan* Tee = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneType == EDiscGolfEnvironmentZoneType::Tee; });
    const FDiscGolfHole1EnvironmentZonePlan* Green = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneType == EDiscGolfEnvironmentZoneType::Green; });
    const FDiscGolfHole1EnvironmentZonePlan* Fairway = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneId == TEXT("H01_PrimaryFairway"); });
    const FDiscGolfHole1EnvironmentZonePlan* Semi = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneType == EDiscGolfEnvironmentZoneType::SemiRough; });
    if (!Tee || !Green || !Fairway || !Semi)
    {
        OutError = TEXT("Hole 1 zone plan is missing tee, green, fairway, or semi-rough");
        return false;
    }
    if (!FMath::IsWithinInclusive(Tee->RadiusCm, 914.4f, 1219.2f)
        || !FMath::IsWithinInclusive(Green->RadiusCm, 914.4f, 1219.2f)
        || !FMath::IsWithinInclusive(Fairway->WidthCm, 1371.6f, 2133.6f)
        || Fairway->TreeSetbackCm <= Fairway->BrushSetbackCm
        || Semi->BlendFalloffCm <= 0.0f)
    {
        OutError = TEXT("Hole 1 clearance dimensions violate the benchmark design contract");
        return false;
    }
    for (const FDiscGolfTreeDefinition& Tree : Definition.Trees)
    {
        if (FVector::Dist2D(Tree.LocationCm, Definition.TeeLocationCm) < Tee->RadiusCm
            || FVector::Dist2D(Tree.LocationCm, Definition.BasketLocationCm) < Green->RadiusCm)
        {
            OutError = TEXT("an authored tree obstructs the tee or 10-meter green");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool PineRidgeHole1Environment::ValidateRepresentativeFlightRoutes(
    const FDiscGolfHoleBlockoutDefinition& Definition,
    FString& OutError)
{
    const TArray<FVector> Primary = RoutePoints(Definition, EDiscGolfShotRouteType::Primary);
    const TArray<FVector> Alternate = RoutePoints(Definition, EDiscGolfShotRouteType::Bailout);
    if (Primary.Num() < 3 || Alternate.Num() < 3)
    {
        OutError = TEXT("Hole 1 requires authored primary and turnover/forehand routes");
        return false;
    }
    const float DirectDistance = FVector::Dist2D(Definition.TeeLocationCm, Definition.BasketLocationCm);
    constexpr float VerifiedDriverCarryCm = 9800.0f;
    if (VerifiedDriverCarryCm < DirectDistance * 0.82f || VerifiedDriverCarryCm > DirectDistance * 1.05f)
    {
        OutError = TEXT("verified driver carry no longer produces a realistic Hole 1 approach");
        return false;
    }

    int32 PrimaryGuardians = 0;
    int32 AlternateGuardians = 0;
    for (const FDiscGolfTreeDefinition& Tree : Definition.Trees)
    {
        if (RouteDistance2D(Tree.LocationCm, Primary) < 1350.0f) ++PrimaryGuardians;
        if (RouteDistance2D(Tree.LocationCm, Alternate) < 1150.0f) ++AlternateGuardians;
    }
    if (!FMath::IsWithinInclusive(PrimaryGuardians, 1, 4)
        || !FMath::IsWithinInclusive(AlternateGuardians, 2, 6))
    {
        OutError = FString::Printf(TEXT("guardian count is not strategic (primary=%d alternate=%d)"),
            PrimaryGuardians, AlternateGuardians);
        return false;
    }
    OutError.Reset();
    return true;
}
