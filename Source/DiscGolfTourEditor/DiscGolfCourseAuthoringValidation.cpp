#include "DiscGolfCourseAuthoringValidation.h"

#include "DGFrameworkCourseDefinition.h"
#include "DiscGolfCourseValidatorLibrary.h"

namespace
{
    constexpr int32 MaxCourseHoles = 36;
    constexpr int32 MaxZonesPerHole = 128;
    constexpr int32 MaxDropZonesPerHole = 32;
    constexpr int32 MaxMandosPerHole = 32;
    constexpr int32 MaxPolygonPoints = 256;
    constexpr int32 PolygonPointWarningThreshold = 128;
    constexpr int32 ZoneWarningThreshold = 64;
    constexpr int32 CoursePointWarningThreshold = 1024;
    constexpr int32 ObstructionGridResolution = 16;
    constexpr double MinimumPolygonAreaCm2 = 10000.0;
    constexpr double TeeBoundaryClearanceCm = 250.0;
    constexpr double BasketBoundaryClearanceCm = 200.0;
    constexpr double DropZoneForbiddenClearanceCm = 200.0;
    constexpr double TeeBasketClearanceCm = 1000.0;
    constexpr double SpectatorSpawnClearanceCm = 1500.0;
    constexpr double FairwayObstructionWarningFraction = 0.15;
    constexpr double FairwayObstructionErrorFraction = 0.35;

    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y)
            && FMath::IsFinite(Value.Z) && !Value.ContainsNaN();
    }

    bool IsFiniteTransform(const FTransform& Value)
    {
        const FQuat Rotation = Value.GetRotation();
        return IsFiniteVector(Value.GetLocation())
            && IsFiniteVector(Value.GetScale3D())
            && FMath::IsFinite(Rotation.X) && FMath::IsFinite(Rotation.Y)
            && FMath::IsFinite(Rotation.Z) && FMath::IsFinite(Rotation.W)
            && !Rotation.ContainsNaN();
    }

    bool IsSupportedZoneType(EDGCourseZoneType ZoneType)
    {
        switch (ZoneType)
        {
        case EDGCourseZoneType::TeeSafety:
        case EDGCourseZoneType::FairwayPrimary:
        case EDGCourseZoneType::FairwaySecondary:
        case EDGCourseZoneType::Rough:
        case EDGCourseZoneType::DeepRough:
        case EDGCourseZoneType::Green:
        case EDGCourseZoneType::OutOfBounds:
        case EDGCourseZoneType::WaterHazard:
        case EDGCourseZoneType::Spectator:
        case EDGCourseZoneType::NoSpawn:
            return true;
        default:
            return false;
        }
    }

    void AddIssue(
        TArray<FDGCourseValidationIssue>& Issues,
        EDGValidationSeverity Severity,
        FName Code,
        int32 HoleNumber,
        const FString& Message)
    {
        const bool bAlreadyReported = Issues.ContainsByPredicate(
            [Severity, Code, HoleNumber](const FDGCourseValidationIssue& Existing)
            {
                return Existing.Severity == Severity && Existing.Code == Code
                    && Existing.HoleNumber == HoleNumber;
            });
        if (bAlreadyReported)
        {
            return;
        }

        FDGCourseValidationIssue& Issue = Issues.AddDefaulted_GetRef();
        Issue.Severity = Severity;
        Issue.Code = Code;
        Issue.HoleNumber = HoleNumber;
        Issue.Message = Message;
    }

    double Cross2D(const FVector& A, const FVector& B, const FVector& C)
    {
        return static_cast<double>(B.X - A.X) * static_cast<double>(C.Y - A.Y)
            - static_cast<double>(B.Y - A.Y) * static_cast<double>(C.X - A.X);
    }

    bool PointOnSegment2D(const FVector& Point, const FVector& A, const FVector& B)
    {
        constexpr double Epsilon = 0.01;
        if (FMath::Abs(Cross2D(A, B, Point)) > Epsilon)
        {
            return false;
        }
        return Point.X >= FMath::Min(A.X, B.X) - Epsilon
            && Point.X <= FMath::Max(A.X, B.X) + Epsilon
            && Point.Y >= FMath::Min(A.Y, B.Y) - Epsilon
            && Point.Y <= FMath::Max(A.Y, B.Y) + Epsilon;
    }

    int32 Orientation2D(const FVector& A, const FVector& B, const FVector& C)
    {
        constexpr double Epsilon = 0.01;
        const double Cross = Cross2D(A, B, C);
        return Cross > Epsilon ? 1 : (Cross < -Epsilon ? -1 : 0);
    }

    bool SegmentsIntersect2D(
        const FVector& A, const FVector& B, const FVector& C, const FVector& D)
    {
        const int32 O1 = Orientation2D(A, B, C);
        const int32 O2 = Orientation2D(A, B, D);
        const int32 O3 = Orientation2D(C, D, A);
        const int32 O4 = Orientation2D(C, D, B);
        if (O1 != O2 && O3 != O4)
        {
            return true;
        }
        return (O1 == 0 && PointOnSegment2D(C, A, B))
            || (O2 == 0 && PointOnSegment2D(D, A, B))
            || (O3 == 0 && PointOnSegment2D(A, C, D))
            || (O4 == 0 && PointOnSegment2D(B, C, D));
    }

    bool IsSelfIntersecting(const TArray<FVector>& Points)
    {
        const int32 Count = Points.Num();
        for (int32 EdgeA = 0; EdgeA < Count; ++EdgeA)
        {
            const int32 EdgeANext = (EdgeA + 1) % Count;
            for (int32 EdgeB = EdgeA + 1; EdgeB < Count; ++EdgeB)
            {
                const int32 EdgeBNext = (EdgeB + 1) % Count;
                if (EdgeA == EdgeB || EdgeANext == EdgeB || EdgeBNext == EdgeA)
                {
                    continue;
                }
                if (SegmentsIntersect2D(
                    Points[EdgeA], Points[EdgeANext], Points[EdgeB], Points[EdgeBNext]))
                {
                    return true;
                }
            }
        }
        return false;
    }

    double PolygonAreaCm2(const TArray<FVector>& Points)
    {
        double TwiceArea = 0.0;
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            const FVector& A = Points[Index];
            const FVector& B = Points[(Index + 1) % Points.Num()];
            TwiceArea += static_cast<double>(A.X) * static_cast<double>(B.Y)
                - static_cast<double>(B.X) * static_cast<double>(A.Y);
        }
        return FMath::Abs(TwiceArea) * 0.5;
    }

    bool IsValidPolygonForGeometry(const TArray<FVector>& Points)
    {
        if (Points.Num() < 3 || Points.Num() > MaxPolygonPoints)
        {
            return false;
        }
        for (const FVector& Point : Points)
        {
            if (!IsFiniteVector(Point))
            {
                return false;
            }
        }
        return PolygonAreaCm2(Points) >= MinimumPolygonAreaCm2
            && !IsSelfIntersecting(Points);
    }

    bool PointInPolygon2D(const FVector& Point, const TArray<FVector>& Polygon)
    {
        bool bInside = false;
        for (int32 Index = 0, Previous = Polygon.Num() - 1;
            Index < Polygon.Num(); Previous = Index++)
        {
            const FVector& A = Polygon[Index];
            const FVector& B = Polygon[Previous];
            if (PointOnSegment2D(Point, A, B))
            {
                return true;
            }
            const bool bCrosses = ((A.Y > Point.Y) != (B.Y > Point.Y))
                && (Point.X < (B.X - A.X) * (Point.Y - A.Y)
                    / (B.Y - A.Y) + A.X);
            if (bCrosses)
            {
                bInside = !bInside;
            }
        }
        return bInside;
    }

    double DistanceToSegment2D(const FVector& Point, const FVector& A, const FVector& B)
    {
        const FVector2D Segment(B.X - A.X, B.Y - A.Y);
        const FVector2D Relative(Point.X - A.X, Point.Y - A.Y);
        const double LengthSquared = Segment.SizeSquared();
        if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER)
        {
            return Relative.Size();
        }
        const double Alpha = FMath::Clamp(
            FVector2D::DotProduct(Relative, Segment) / LengthSquared, 0.0, 1.0);
        return (Relative - Segment * Alpha).Size();
    }

    double DistanceToPolygonBoundary2D(const FVector& Point, const TArray<FVector>& Polygon)
    {
        double Minimum = TNumericLimits<double>::Max();
        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            Minimum = FMath::Min(Minimum, DistanceToSegment2D(
                Point, Polygon[Index], Polygon[(Index + 1) % Polygon.Num()]));
        }
        return Minimum;
    }

    bool PolygonsOverlap2D(const TArray<FVector>& A, const TArray<FVector>& B)
    {
        for (int32 EdgeA = 0; EdgeA < A.Num(); ++EdgeA)
        {
            for (int32 EdgeB = 0; EdgeB < B.Num(); ++EdgeB)
            {
                if (SegmentsIntersect2D(A[EdgeA], A[(EdgeA + 1) % A.Num()],
                    B[EdgeB], B[(EdgeB + 1) % B.Num()]))
                {
                    return true;
                }
            }
        }
        return PointInPolygon2D(A[0], B) || PointInPolygon2D(B[0], A);
    }

    bool IsForbiddenZone(EDGCourseZoneType Type)
    {
        return Type == EDGCourseZoneType::OutOfBounds
            || Type == EDGCourseZoneType::WaterHazard
            || Type == EDGCourseZoneType::NoSpawn;
    }

    bool IsHardClearanceZone(EDGCourseZoneType Type)
    {
        return Type == EDGCourseZoneType::TeeSafety
            || Type == EDGCourseZoneType::Green
            || Type == EDGCourseZoneType::WaterHazard
            || Type == EDGCourseZoneType::Spectator
            || Type == EDGCourseZoneType::NoSpawn;
    }

    bool IsInsideAnyZone(
        const FVector& Point,
        const TArray<const FDGCourseZoneDefinition*>& Zones)
    {
        for (const FDGCourseZoneDefinition* Zone : Zones)
        {
            if (PointInPolygon2D(Point, Zone->PolygonPointsCm))
            {
                return true;
            }
        }
        return false;
    }

    bool HasClearanceInsideAnyZone(
        const FVector& Point,
        const TArray<const FDGCourseZoneDefinition*>& Zones,
        double RequiredClearanceCm)
    {
        for (const FDGCourseZoneDefinition* Zone : Zones)
        {
            if (PointInPolygon2D(Point, Zone->PolygonPointsCm)
                && DistanceToPolygonBoundary2D(Point, Zone->PolygonPointsCm) >= RequiredClearanceCm)
            {
                return true;
            }
        }
        return false;
    }

    double FairwayObstructionFraction(
        const FDGCourseZoneDefinition& Fairway,
        const TArray<const FDGCourseZoneDefinition*>& ForbiddenZones)
    {
        FBox2D Bounds(ForceInit);
        for (const FVector& Point : Fairway.PolygonPointsCm)
        {
            Bounds += FVector2D(Point.X, Point.Y);
        }
        if (!Bounds.bIsValid || Bounds.GetArea() <= 0.0)
        {
            return 1.0;
        }

        int32 FairwaySamples = 0;
        int32 ObstructedSamples = 0;
        for (int32 Y = 0; Y < ObstructionGridResolution; ++Y)
        {
            for (int32 X = 0; X < ObstructionGridResolution; ++X)
            {
                const FVector2D Alpha(
                    (static_cast<double>(X) + 0.5) / ObstructionGridResolution,
                    (static_cast<double>(Y) + 0.5) / ObstructionGridResolution);
                const FVector2D Sample2D(
                    FMath::Lerp(Bounds.Min.X, Bounds.Max.X, Alpha.X),
                    FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, Alpha.Y));
                const FVector Sample(Sample2D.X, Sample2D.Y, 0.0);
                if (!PointInPolygon2D(Sample, Fairway.PolygonPointsCm))
                {
                    continue;
                }
                ++FairwaySamples;
                if (IsInsideAnyZone(Sample, ForbiddenZones))
                {
                    ++ObstructedSamples;
                }
            }
        }
        return FairwaySamples > 0
            ? static_cast<double>(ObstructedSamples) / FairwaySamples
            : 1.0;
    }

    bool IsNearPoint2D(const FDGCourseZoneDefinition& Zone, const FVector& Point, double DistanceCm)
    {
        return PointInPolygon2D(Point, Zone.PolygonPointsCm)
            || DistanceToPolygonBoundary2D(Point, Zone.PolygonPointsCm) < DistanceCm;
    }
}

TArray<FDGCourseValidationIssue> UDiscGolfCourseAuthoringValidation::ValidateCourse(
    const UDiscGolfCourseDefinition* Course)
{
    TArray<FDGCourseValidationIssue> Issues =
        UDiscGolfCourseValidatorLibrary::ValidateCourse(Course);
    if (!Course || Course->Holes.IsEmpty())
    {
        return Issues;
    }

    int32 CoursePointCount = 0;
    if (Course->Holes.Num() > MaxCourseHoles)
    {
        AddIssue(Issues, EDGValidationSeverity::Error, TEXT("COURSE_HOLE_BUDGET_EXCEEDED"),
            0, FString::Printf(
                TEXT("Course exceeds the deterministic %d-hole authoring-validation bound."),
                MaxCourseHoles));
    }

    const int32 HoleCount = FMath::Min(Course->Holes.Num(), MaxCourseHoles);
    for (int32 HoleIndex = 0; HoleIndex < HoleCount; ++HoleIndex)
    {
        const FDGHoleDefinition& Hole = Course->Holes[HoleIndex];
        if (Hole.Par < 1)
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("HOLE_PAR_INVALID"),
                Hole.HoleNumber, TEXT("Par must be greater than zero."));
        }
        if (!FMath::IsFinite(Hole.PublishedDistanceM))
        {
            AddIssue(Issues, EDGValidationSeverity::Error,
                TEXT("HOLE_PUBLISHED_DISTANCE_NON_FINITE"), Hole.HoleNumber,
                TEXT("Published distance contains a non-finite value."));
        }
        else if (Hole.PublishedDistanceM < 0.0f)
        {
            AddIssue(Issues, EDGValidationSeverity::Error,
                TEXT("HOLE_PUBLISHED_DISTANCE_NEGATIVE"), Hole.HoleNumber,
                TEXT("Published distance must be non-negative."));
        }
        if (!FMath::IsFinite(Hole.ElevationChangeM))
        {
            AddIssue(Issues, EDGValidationSeverity::Error,
                TEXT("HOLE_ELEVATION_CHANGE_NON_FINITE"), Hole.HoleNumber,
                TEXT("Elevation change contains a non-finite value."));
        }
        if (!IsFiniteTransform(Hole.TeeTransform))
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("TEE_TRANSFORM_NON_FINITE"),
                Hole.HoleNumber, TEXT("Tee transform contains a non-finite value."));
        }
        if (!IsFiniteTransform(Hole.BasketTransform))
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("BASKET_TRANSFORM_NON_FINITE"),
                Hole.HoleNumber, TEXT("Basket transform contains a non-finite value."));
        }
        if (Hole.DropZoneTransforms.Num() > MaxDropZonesPerHole)
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("DROPZONE_BUDGET_EXCEEDED"),
                Hole.HoleNumber, FString::Printf(
                    TEXT("Hole exceeds the deterministic %d-drop-zone validation bound."),
                    MaxDropZonesPerHole));
        }
        const int32 DropZoneCount = FMath::Min(
            Hole.DropZoneTransforms.Num(), MaxDropZonesPerHole);
        for (int32 DropZoneIndex = 0; DropZoneIndex < DropZoneCount; ++DropZoneIndex)
        {
            if (!IsFiniteTransform(Hole.DropZoneTransforms[DropZoneIndex]))
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("DROPZONE_TRANSFORM_NON_FINITE"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Drop zone %d contains a non-finite transform."), DropZoneIndex));
            }
        }

        if (Hole.Zones.Num() > ZoneWarningThreshold)
        {
            AddIssue(Issues, EDGValidationSeverity::Warning, TEXT("ZONE_COUNT_PERFORMANCE_WARNING"),
                Hole.HoleNumber, FString::Printf(
                    TEXT("Hole has %d zones; profile editor and runtime zone consumers."), Hole.Zones.Num()));
        }
        if (Hole.Zones.Num() > MaxZonesPerHole)
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("ZONE_BUDGET_EXCEEDED"),
                Hole.HoleNumber, FString::Printf(
                    TEXT("Hole exceeds the deterministic %d-zone validation bound."),
                    MaxZonesPerHole));
        }
        if (Hole.Mandos.Num() > MaxMandosPerHole)
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("MANDO_BUDGET_EXCEEDED"),
                Hole.HoleNumber, FString::Printf(
                    TEXT("Hole exceeds the deterministic %d-mando validation bound."),
                    MaxMandosPerHole));
        }

        TArray<const FDGCourseZoneDefinition*> TeeZones;
        TArray<const FDGCourseZoneDefinition*> GreenZones;
        TArray<const FDGCourseZoneDefinition*> FairwayZones;
        TArray<const FDGCourseZoneDefinition*> ForbiddenZones;
        TArray<const FDGCourseZoneDefinition*> SpectatorZones;
        int32 ObZoneCount = 0;

        const int32 ZoneCount = FMath::Min(Hole.Zones.Num(), MaxZonesPerHole);
        for (int32 ZoneIndex = 0; ZoneIndex < ZoneCount; ++ZoneIndex)
        {
            const FDGCourseZoneDefinition& Zone = Hole.Zones[ZoneIndex];
            CoursePointCount += Zone.PolygonPointsCm.Num();
            if (!IsSupportedZoneType(Zone.ZoneType))
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("ZONE_TYPE_INVALID"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Zone %s has an unsupported zone type."),
                        *Zone.ZoneId.ToString()));
            }
            if (Zone.PenaltyStrokes < 0)
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("ZONE_PENALTY_INVALID"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Zone %s has a negative penalty."),
                        *Zone.ZoneId.ToString()));
            }
            if (IsHardClearanceZone(Zone.ZoneType) && !Zone.bAffectsVegetation)
            {
                AddIssue(Issues, EDGValidationSeverity::Error,
                    TEXT("HARD_CLEARANCE_VEGETATION_DISABLED"), Hole.HoleNumber,
                    FString::Printf(
                        TEXT("Hard-clearance zone %s cannot disable vegetation influence."),
                        *Zone.ZoneId.ToString()));
            }
            if (Zone.PolygonPointsCm.Num() > MaxPolygonPoints)
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("ZONE_POINT_BUDGET_EXCEEDED"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Zone %s exceeds the deterministic %d-point validation bound."),
                        *Zone.ZoneId.ToString(), MaxPolygonPoints));
                continue;
            }
            if (Zone.PolygonPointsCm.Num() > PolygonPointWarningThreshold)
            {
                AddIssue(Issues, EDGValidationSeverity::Warning, TEXT("ZONE_POINT_PERFORMANCE_WARNING"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Zone %s has %d polygon points; profile authoring and query cost."),
                        *Zone.ZoneId.ToString(), Zone.PolygonPointsCm.Num()));
            }

            bool bFinite = true;
            for (const FVector& Point : Zone.PolygonPointsCm)
            {
                if (!IsFiniteVector(Point))
                {
                    bFinite = false;
                    break;
                }
            }
            if (!bFinite)
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("ZONE_POINT_NON_FINITE"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Zone %s contains a non-finite polygon point."), *Zone.ZoneId.ToString()));
                continue;
            }
            const bool bDegenerate = Zone.PolygonPointsCm.Num() >= 3
                && PolygonAreaCm2(Zone.PolygonPointsCm) < MinimumPolygonAreaCm2;
            const bool bSelfIntersecting = Zone.PolygonPointsCm.Num() >= 3
                && IsSelfIntersecting(Zone.PolygonPointsCm);
            if (bDegenerate)
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("ZONE_POLYGON_DEGENERATE"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Zone %s has less than one square meter of planar area."),
                        *Zone.ZoneId.ToString()));
            }
            if (bSelfIntersecting)
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("ZONE_POLYGON_SELF_INTERSECTS"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Zone %s polygon intersects itself."), *Zone.ZoneId.ToString()));
            }
            if (bDegenerate || bSelfIntersecting)
            {
                continue;
            }
            if (!IsValidPolygonForGeometry(Zone.PolygonPointsCm))
            {
                continue;
            }

            if (Zone.ZoneType == EDGCourseZoneType::TeeSafety) TeeZones.Add(&Zone);
            if (Zone.ZoneType == EDGCourseZoneType::Green) GreenZones.Add(&Zone);
            if (Zone.ZoneType == EDGCourseZoneType::FairwayPrimary) FairwayZones.Add(&Zone);
            if (Zone.ZoneType == EDGCourseZoneType::Spectator) SpectatorZones.Add(&Zone);
            if (IsForbiddenZone(Zone.ZoneType)) ForbiddenZones.Add(&Zone);
            if (Zone.ZoneType == EDGCourseZoneType::OutOfBounds) ++ObZoneCount;
        }

        const FVector TeeLocation = Hole.TeeTransform.GetLocation();
        const FVector BasketLocation = Hole.BasketTransform.GetLocation();
        if (IsFiniteVector(TeeLocation) && IsFiniteVector(BasketLocation)
            && FVector::Dist2D(TeeLocation, BasketLocation) < TeeBasketClearanceCm)
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("TEE_BASKET_CLEARANCE_CONFLICT"),
                Hole.HoleNumber, TEXT("Tee and basket collision clearances overlap."));
        }

        if (TeeZones.IsEmpty())
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("TEE_SAFETY_ZONE_MISSING"),
                Hole.HoleNumber, TEXT("A tee safety zone is required."));
        }
        else if (IsFiniteVector(TeeLocation)
            && !HasClearanceInsideAnyZone(TeeLocation, TeeZones, TeeBoundaryClearanceCm))
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("TEE_SPAWN_CLEARANCE_UNSAFE"),
                Hole.HoleNumber, TEXT("Tee spawn lacks 2.5 meters of tee-zone boundary clearance."));
        }

        if (GreenZones.IsEmpty())
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("GREEN_ZONE_MISSING"),
                Hole.HoleNumber, TEXT("A basket green zone is required."));
        }
        else if (IsFiniteVector(BasketLocation)
            && !HasClearanceInsideAnyZone(BasketLocation, GreenZones, BasketBoundaryClearanceCm))
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("BASKET_ZONE_CLEARANCE_UNSAFE"),
                Hole.HoleNumber, TEXT("Basket lacks 2 meters of green-zone boundary clearance."));
        }

        if (FairwayZones.IsEmpty())
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("PRIMARY_FAIRWAY_MISSING"),
                Hole.HoleNumber, TEXT("A primary fairway zone is required."));
        }

        if (IsFiniteVector(TeeLocation) && IsInsideAnyZone(TeeLocation, ForbiddenZones))
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("TEE_FORBIDDEN_ZONE_CONFLICT"),
                Hole.HoleNumber, TEXT("Tee spawn intersects OB, water, or a no-spawn zone."));
        }
        if (IsFiniteVector(BasketLocation) && IsInsideAnyZone(BasketLocation, ForbiddenZones))
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("BASKET_FORBIDDEN_ZONE_CONFLICT"),
                Hole.HoleNumber, TEXT("Basket collision location intersects OB, water, or a no-spawn zone."));
        }

        for (const FDGCourseZoneDefinition* TeeZone : TeeZones)
        {
            for (const FDGCourseZoneDefinition* GreenZone : GreenZones)
            {
                if (PolygonsOverlap2D(TeeZone->PolygonPointsCm, GreenZone->PolygonPointsCm))
                {
                    AddIssue(Issues, EDGValidationSeverity::Error, TEXT("TEE_GREEN_ZONE_CONFLICT"),
                        Hole.HoleNumber, TEXT("Tee-safety and basket-green polygons overlap."));
                }
            }
            for (const FDGCourseZoneDefinition* Forbidden : ForbiddenZones)
            {
                if (PolygonsOverlap2D(TeeZone->PolygonPointsCm, Forbidden->PolygonPointsCm))
                {
                    AddIssue(Issues, EDGValidationSeverity::Error, TEXT("TEE_ZONE_FORBIDDEN_CONFLICT"),
                        Hole.HoleNumber, TEXT("A forbidden polygon intersects tee safety clearance."));
                }
            }
        }
        for (const FDGCourseZoneDefinition* GreenZone : GreenZones)
        {
            for (const FDGCourseZoneDefinition* Forbidden : ForbiddenZones)
            {
                if (PolygonsOverlap2D(GreenZone->PolygonPointsCm, Forbidden->PolygonPointsCm))
                {
                    AddIssue(Issues, EDGValidationSeverity::Error, TEXT("GREEN_ZONE_FORBIDDEN_CONFLICT"),
                        Hole.HoleNumber, TEXT("A forbidden polygon intersects basket green clearance."));
                }
            }
        }
        for (int32 ForbiddenA = 0; ForbiddenA < ForbiddenZones.Num(); ++ForbiddenA)
        {
            for (int32 ForbiddenB = ForbiddenA + 1; ForbiddenB < ForbiddenZones.Num(); ++ForbiddenB)
            {
                if (ForbiddenZones[ForbiddenA]->ZoneType != ForbiddenZones[ForbiddenB]->ZoneType
                    && PolygonsOverlap2D(ForbiddenZones[ForbiddenA]->PolygonPointsCm,
                        ForbiddenZones[ForbiddenB]->PolygonPointsCm))
                {
                    AddIssue(Issues, EDGValidationSeverity::Error, TEXT("FORBIDDEN_ZONE_CONFLICT"),
                        Hole.HoleNumber,
                        TEXT("Overlapping forbidden-zone types create ambiguous authored rules."));
                }
            }
        }

        if (ObZoneCount > 0 && Hole.DropZoneTransforms.IsEmpty())
        {
            AddIssue(Issues, EDGValidationSeverity::Error, TEXT("OB_DROPZONE_MISSING"),
                Hole.HoleNumber,
                TEXT("OB is authored but the framework schema has no viable drop-zone target."));
        }
        for (int32 Index = 0; Index < DropZoneCount; ++Index)
        {
            const FVector Location = Hole.DropZoneTransforms[Index].GetLocation();
            if (!IsFiniteVector(Location))
            {
                continue;
            }
            bool bUnsafe = false;
            for (const FDGCourseZoneDefinition* Forbidden : ForbiddenZones)
            {
                if (PointInPolygon2D(Location, Forbidden->PolygonPointsCm)
                    || DistanceToPolygonBoundary2D(Location, Forbidden->PolygonPointsCm)
                        < DropZoneForbiddenClearanceCm)
                {
                    bUnsafe = true;
                    break;
                }
            }
            if (bUnsafe)
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("DROPZONE_FORBIDDEN_CONFLICT"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Drop zone %d lacks 2 meters of forbidden-zone clearance."), Index));
            }
        }

        const int32 MandoCount = FMath::Min(Hole.Mandos.Num(), MaxMandosPerHole);
        for (int32 MandoIndex = 0; MandoIndex < MandoCount; ++MandoIndex)
        {
            const FDGMandoDefinition& Mando = Hole.Mandos[MandoIndex];
            if (!IsFiniteVector(Mando.GatePointACm) || !IsFiniteVector(Mando.GatePointBCm)
                || !IsFiniteVector(Mando.RequiredPassDirection))
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("MANDO_TRANSFORM_NON_FINITE"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Mando %s contains non-finite gate data."), *Mando.MandoId.ToString()));
            }
            else if (Mando.RequiredPassDirection.IsNearlyZero())
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("MANDO_DIRECTION_INVALID"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Mando %s has no required pass direction."),
                        *Mando.MandoId.ToString()));
            }
            if (Mando.MissPenaltyStrokes < 0)
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("MANDO_PENALTY_INVALID"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Mando %s has a negative miss penalty."),
                        *Mando.MandoId.ToString()));
            }
            if (Mando.DropZoneIndex != INDEX_NONE
                && !Hole.DropZoneTransforms.IsValidIndex(Mando.DropZoneIndex))
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("MANDO_DROPZONE_INVALID"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Mando %s references invalid drop zone %d."),
                        *Mando.MandoId.ToString(), Mando.DropZoneIndex));
            }
        }

        for (const FDGCourseZoneDefinition* Fairway : FairwayZones)
        {
            const double Obstruction = FairwayObstructionFraction(*Fairway, ForbiddenZones);
            if (Obstruction > FairwayObstructionErrorFraction)
            {
                AddIssue(Issues, EDGValidationSeverity::Error, TEXT("FAIRWAY_OBSTRUCTION_EXCESSIVE"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Primary fairway %.0f%% obstruction exceeds the 35%% limit."),
                        Obstruction * 100.0));
            }
            else if (Obstruction > FairwayObstructionWarningFraction)
            {
                AddIssue(Issues, EDGValidationSeverity::Warning, TEXT("FAIRWAY_OBSTRUCTION_WARNING"),
                    Hole.HoleNumber, FString::Printf(
                        TEXT("Primary fairway is %.0f%% obstructed by forbidden zones."),
                        Obstruction * 100.0));
            }
        }

        if (SpectatorZones.IsEmpty())
        {
            AddIssue(Issues, EDGValidationSeverity::Warning, TEXT("SPECTATOR_ZONE_MISSING"),
                Hole.HoleNumber, TEXT("No spectator boundary is authored for visual safety review."));
        }
        for (const FDGCourseZoneDefinition* Spectator : SpectatorZones)
        {
            if ((IsFiniteVector(TeeLocation)
                    && IsNearPoint2D(*Spectator, TeeLocation, SpectatorSpawnClearanceCm))
                || (IsFiniteVector(BasketLocation)
                    && IsNearPoint2D(*Spectator, BasketLocation, SpectatorSpawnClearanceCm)))
            {
                AddIssue(Issues, EDGValidationSeverity::Warning, TEXT("SPECTATOR_CLEARANCE_WARNING"),
                    Hole.HoleNumber,
                    TEXT("Spectator boundary is within 15 meters of tee or basket review clearance."));
            }
            for (const FDGCourseZoneDefinition* Fairway : FairwayZones)
            {
                if (PolygonsOverlap2D(Spectator->PolygonPointsCm, Fairway->PolygonPointsCm))
                {
                    AddIssue(Issues, EDGValidationSeverity::Warning, TEXT("SPECTATOR_FAIRWAY_WARNING"),
                        Hole.HoleNumber,
                        TEXT("Spectator boundary intersects the primary fairway flight corridor."));
                }
            }
        }
    }

    if (CoursePointCount > CoursePointWarningThreshold)
    {
        AddIssue(Issues, EDGValidationSeverity::Warning, TEXT("COURSE_POINT_PERFORMANCE_WARNING"),
            0, FString::Printf(
                TEXT("Course has %d authored polygon points; profile editor and runtime queries."),
                CoursePointCount));
    }

    Issues.StableSort([](const FDGCourseValidationIssue& A, const FDGCourseValidationIssue& B)
    {
        if (A.HoleNumber != B.HoleNumber) return A.HoleNumber < B.HoleNumber;
        if (A.Severity != B.Severity)
        {
            return static_cast<uint8>(A.Severity) > static_cast<uint8>(B.Severity);
        }
        return A.Code.LexicalLess(B.Code);
    });
    return Issues;
}

bool UDiscGolfCourseAuthoringValidation::HasErrors(
    const TArray<FDGCourseValidationIssue>& Issues)
{
    return UDiscGolfCourseValidatorLibrary::HasErrors(Issues);
}
