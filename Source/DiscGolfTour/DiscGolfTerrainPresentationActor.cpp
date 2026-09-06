#include "DiscGolfTerrainPresentationActor.h"

#include "DiscGolfCourseDefinition.h"
#include "DiscGolfTerrainPresentationMath.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
    constexpr float BasicShapeHalfExtentCm = 50.0f;
    constexpr float CourseMarginCm = 9000.0f;
    constexpr float GridSpacingCm = 400.0f;
    constexpr float GroundTextureWorldScaleCm = 200.0f;

    struct FTerrainPath
    {
        TArray<FVector> Points;
        float HalfWidthCm = 800.0f;
        bool bConnector = false;
        int32 HoleNumber = 0;
    };

    struct FPathSample
    {
        float DistanceCm = TNumericLimits<float>::Max();
        float HeightCm = 0.0f;
        float HalfWidthCm = 800.0f;
    };

    float SmoothStep(float Min, float Max, float Value)
    {
        const float T = FMath::Clamp((Value - Min)
            / FMath::Max(Max - Min, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    uint8 GroundMacroVariation(const FVector2D& Point, float Phase)
    {
        // Three incommensurate world-space waves keep the breakup stable across the full
        // property without exposing the rectangular procedural-mesh UV bounds.
        const float Variation = 0.50f
            + 0.22f * FMath::Sin(Point.X * 0.00037f + Phase)
            + 0.17f * FMath::Sin(Point.Y * 0.00053f - Phase * 0.71f)
            + 0.11f * FMath::Sin((Point.X - Point.Y) * 0.00121f + Phase * 1.63f);
        return static_cast<uint8>(FMath::RoundToInt(
            255.0f * FMath::Clamp(Variation, 0.0f, 1.0f)));
    }

    float SurfaceTop(const FDiscGolfBlockoutSurfaceDefinition& Surface)
    {
        return Surface.LocationCm.Z + BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.Z);
    }

    const FDiscGolfShotRouteDefinition* FindPrimaryRoute(
        const FDiscGolfHoleBlockoutDefinition& Definition)
    {
        return Definition.ShotRoutes.FindByPredicate(
            [](const FDiscGolfShotRouteDefinition& Route)
            {
                return Route.RouteType == EDiscGolfShotRouteType::Primary;
            });
    }

    TArray<FTerrainPath> BuildCoursePaths(
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions)
    {
        TArray<FTerrainPath> Paths;
        for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
        {
            FTerrainPath HolePath;
            HolePath.HoleNumber = Definition.HoleNumber;
            if (const FDiscGolfShotRouteDefinition* Primary = FindPrimaryRoute(Definition))
            {
                HolePath.Points = Primary->WaypointsCm;
                const float PrimaryHalfWidthCm = Primary->CorridorWidthCm * 0.5f;
                if (FMath::IsFinite(PrimaryHalfWidthCm)
                    && PrimaryHalfWidthCm > 0.0f)
                {
                    HolePath.HalfWidthCm = FMath::Max(
                        450.0f, PrimaryHalfWidthCm);
                }
            }
            if (HolePath.Points.Num() < 2)
            {
                HolePath.Points = { Definition.TeeLocationCm, Definition.BasketLocationCm };
                HolePath.HalfWidthCm = 800.0f;
            }
            // Strategy-route waypoints describe an elevated shot corridor, not
            // the physical ground. Anchor that corridor back to the authored tee
            // and basket elevations while preserving the residual interior
            // elevation profile. Equal endpoint clearance preserves every grade.
            // Pine Ridge routes are +20 cm at both ends; deriving the clearance
            // keeps fallback and future non-uniform paths correct as well.
            TArray<FVector> NormalizedPoints;
            if (!DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
                    HolePath.Points,
                    Definition.TeeLocationCm,
                    Definition.BasketLocationCm,
                    NormalizedPoints))
            {
                // ConfigureCourse is also a native boundary. A malformed route
                // may fall back to the two trusted anchors, but malformed or
                // degenerate anchors fail the entire terrain build closed.
                const TArray<FVector> FallbackPoints = {
                    Definition.TeeLocationCm, Definition.BasketLocationCm };
                if (!DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
                        FallbackPoints,
                        Definition.TeeLocationCm,
                        Definition.BasketLocationCm,
                        NormalizedPoints))
                {
                    return {};
                }
                HolePath.HalfWidthCm = 800.0f;
            }
            HolePath.Points = MoveTemp(NormalizedPoints);
            Paths.Add(MoveTemp(HolePath));
        }

        for (int32 Index = 1; Index < Definitions.Num(); ++Index)
        {
            const FVector Start = Definitions[Index - 1].BasketLocationCm;
            const FVector End = Definitions[Index].TeeLocationCm;
            const FVector2D Delta2D(End.X - Start.X, End.Y - Start.Y);
            const FVector2D Side = FVector2D(-Delta2D.Y, Delta2D.X).GetSafeNormal();
            const float BendSign = Index % 2 == 0 ? -1.0f : 1.0f;
            const FVector Bend(Side.X * 900.0f * BendSign, Side.Y * 900.0f * BendSign, 0.0f);

            FTerrainPath Connector;
            Connector.bConnector = true;
            Connector.HalfWidthCm = 260.0f;
            Connector.Points = {
                Start,
                FMath::Lerp(Start, End, 0.34f) + Bend,
                FMath::Lerp(Start, End, 0.68f) + Bend * 0.55f,
                End
            };
            TArray<FVector> NormalizedPoints;
            if (!DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
                    Connector.Points, Start, End, NormalizedPoints))
            {
                return {};
            }
            Connector.Points = MoveTemp(NormalizedPoints);
            Paths.Add(MoveTemp(Connector));
        }
        return Paths;
    }

    FPathSample SampleNearestPath(const FVector2D& Point, const TArray<FTerrainPath>& Paths)
    {
        FPathSample Best;
        for (const FTerrainPath& Path : Paths)
        {
            for (int32 Index = 1; Index < Path.Points.Num(); ++Index)
            {
                const FVector Start = Path.Points[Index - 1];
                const FVector End = Path.Points[Index];
                const FVector2D Start2D(Start.X, Start.Y);
                const FVector2D End2D(End.X, End.Y);
                const FVector2D Segment = End2D - Start2D;
                const float SegmentLengthSquared = Segment.SizeSquared();
                if (SegmentLengthSquared <= KINDA_SMALL_NUMBER) continue;
                const float Alpha = FMath::Clamp(
                    FVector2D::DotProduct(Point - Start2D, Segment) / SegmentLengthSquared,
                    0.0f, 1.0f);
                const float Distance = FVector2D::Distance(Point, Start2D + Segment * Alpha);
                if (Distance < Best.DistanceCm)
                {
                    Best.DistanceCm = Distance;
                    Best.HeightCm = FMath::Lerp(Start.Z, End.Z, Alpha);
                    Best.HalfWidthCm = Path.HalfWidthCm;
                }
            }
        }
        return Best;
    }

    bool IsLakeSurface(const FDiscGolfBlockoutSurfaceDefinition& Surface)
    {
        return Surface.SurfaceId.ToString().Contains(TEXT("LakeWater"));
    }

    float ApplyLakeBasin(
        const FVector2D& Point,
        float Height,
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions)
    {
        for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
        {
            for (const FDiscGolfBlockoutSurfaceDefinition& Surface : Definition.Surfaces)
            {
                if (!IsLakeSurface(Surface)) continue;
                const FVector Local = Surface.Rotation.UnrotateVector(
                    FVector(Point.X, Point.Y, Surface.LocationCm.Z) - Surface.LocationCm);
                const float RadiusX = FMath::Max(100.0f,
                    BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.X));
                const float RadiusY = FMath::Max(100.0f,
                    BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.Y));
                const float Distance = FMath::Sqrt(
                    FMath::Square(Local.X / RadiusX) + FMath::Square(Local.Y / RadiusY));
                const float BasinBlend = 1.0f - SmoothStep(0.82f, 1.32f, Distance);
                const float LakeBed = SurfaceTop(Surface) - 62.0f;
                Height = FMath::Lerp(Height, FMath::Min(Height, LakeBed), BasinBlend);
            }
        }
        return Height;
    }

    float CourseHeightAt(
        const FVector2D& Point,
        const TArray<FTerrainPath>& Paths,
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
        float Phase)
    {
        const FPathSample Path = SampleNearestPath(Point, Paths);
        if (!FMath::IsFinite(Path.DistanceCm)) return 0.0f;
        const float OffRoute = SmoothStep(Path.HalfWidthCm * 0.75f, 8200.0f, Path.DistanceCm);
        const float BroadWave = FMath::Sin(Point.X * 0.00031f + Phase)
            + 0.62f * FMath::Sin(Point.Y * 0.00047f - Phase * 0.73f)
            + 0.28f * FMath::Sin((Point.X + Point.Y) * 0.00083f + Phase * 1.9f);
        float Height = Path.HeightCm + OffRoute * (82.0f + 58.0f * BroadWave);
        return ApplyLakeBasin(Point, Height, Definitions);
    }

    FVector CourseNormalAt(
        const FVector2D& Point,
        const TArray<FTerrainPath>& Paths,
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
        float Phase)
    {
        constexpr float SampleDistanceCm = 90.0f;
        const float Left = CourseHeightAt(Point - FVector2D(SampleDistanceCm, 0.0f),
            Paths, Definitions, Phase);
        const float Right = CourseHeightAt(Point + FVector2D(SampleDistanceCm, 0.0f),
            Paths, Definitions, Phase);
        const float Back = CourseHeightAt(Point - FVector2D(0.0f, SampleDistanceCm),
            Paths, Definitions, Phase);
        const float Front = CourseHeightAt(Point + FVector2D(0.0f, SampleDistanceCm),
            Paths, Definitions, Phase);
        const FVector TangentX(2.0f * SampleDistanceCm, 0.0f, Right - Left);
        const FVector TangentY(0.0f, 2.0f * SampleDistanceCm, Front - Back);
        return FVector::CrossProduct(TangentX, TangentY)
            .GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    }

    bool SamplePolyline(
        const TArray<FVector>& Points,
        float NormalizedDistance,
        FVector& OutPoint,
        FVector2D& OutTangent)
    {
        float TotalLength = 0.0f;
        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            TotalLength += FVector2D::Distance(FVector2D(Points[Index - 1]), FVector2D(Points[Index]));
        }
        if (Points.Num() < 2 || TotalLength <= KINDA_SMALL_NUMBER) return false;

        const float Target = FMath::Clamp(NormalizedDistance, 0.0f, 1.0f) * TotalLength;
        float Traversed = 0.0f;
        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            const FVector Start = Points[Index - 1];
            const FVector End = Points[Index];
            const float Length = FVector2D::Distance(FVector2D(Start), FVector2D(End));
            if (Length <= KINDA_SMALL_NUMBER) continue;
            if (Traversed + Length >= Target || Index == Points.Num() - 1)
            {
                const float Alpha = FMath::Clamp((Target - Traversed) / Length, 0.0f, 1.0f);
                OutPoint = FMath::Lerp(Start, End, Alpha);
                OutTangent = (FVector2D(End) - FVector2D(Start)).GetSafeNormal();
                return !OutTangent.IsNearlyZero();
            }
            Traversed += Length;
        }
        return false;
    }

    bool IsProtectedGrassCandidate(
        const FVector2D& Candidate,
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions)
    {
        for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
        {
            if (FVector2D::Distance(Candidate, FVector2D(Definition.TeeLocationCm)) < 850.0f
                || FVector2D::Distance(Candidate, FVector2D(Definition.BasketLocationCm)) < 650.0f)
            {
                return true;
            }
            for (const FDiscGolfCameraAnchorDefinition& Camera : Definition.CameraAnchors)
            {
                if (FVector2D::Distance(Candidate, FVector2D(Camera.LocationCm)) < 550.0f)
                {
                    return true;
                }
            }
            for (const FDiscGolfSpectatorBoundaryDefinition& Boundary : Definition.SpectatorBoundaries)
            {
                const FVector Local = Boundary.Rotation.UnrotateVector(
                    FVector(Candidate.X, Candidate.Y, Boundary.LocationCm.Z) - Boundary.LocationCm);
                if (FMath::Abs(Local.X) <= Boundary.ExtentCm.X
                    && FMath::Abs(Local.Y) <= Boundary.ExtentCm.Y)
                {
                    return true;
                }
            }
            for (const FDiscGolfBlockoutSurfaceDefinition& Surface : Definition.Surfaces)
            {
                if (!IsLakeSurface(Surface)) continue;
                const FVector Local = Surface.Rotation.UnrotateVector(
                    FVector(Candidate.X, Candidate.Y, Surface.LocationCm.Z) - Surface.LocationCm);
                const float RadiusX = BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.X) + 1600.0f;
                const float RadiusY = BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.Y) + 1600.0f;
                if (FMath::Square(Local.X / RadiusX) + FMath::Square(Local.Y / RadiusY) <= 1.0f)
                {
                    return true;
                }
            }
        }
        return false;
    }

    float NearestAuthoredRouteDistanceCm(
        const FVector2D& Candidate,
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions)
    {
        float BestDistance = TNumericLimits<float>::Max();
        for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
        {
            for (const FDiscGolfShotRouteDefinition& Route : Definition.ShotRoutes)
            {
                for (int32 Index = 1; Index < Route.WaypointsCm.Num(); ++Index)
                {
                    const FVector2D Start(Route.WaypointsCm[Index - 1]);
                    const FVector2D End(Route.WaypointsCm[Index]);
                    const FVector2D Delta = End - Start;
                    const float LengthSquared = Delta.SizeSquared();
                    if (LengthSquared <= KINDA_SMALL_NUMBER) continue;
                    const float Alpha = FMath::Clamp(
                        FVector2D::DotProduct(Candidate - Start, Delta) / LengthSquared,
                        0.0f, 1.0f);
                    BestDistance = FMath::Min(BestDistance,
                        FVector2D::Distance(Candidate, Start + Delta * Alpha));
                }
            }
        }
        return BestDistance;
    }

    bool IsProtectedShoreCandidate(
        const FVector2D& Candidate,
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
        float MinimumRouteClearanceCm)
    {
        if (NearestAuthoredRouteDistanceCm(Candidate, Definitions) < MinimumRouteClearanceCm)
        {
            return true;
        }
        for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
        {
            if (FVector2D::Distance(Candidate, FVector2D(Definition.TeeLocationCm)) < 1200.0f
                || FVector2D::Distance(Candidate, FVector2D(Definition.BasketLocationCm)) < 1500.0f)
            {
                return true;
            }
            for (const FDiscGolfCameraAnchorDefinition& Camera : Definition.CameraAnchors)
            {
                if (FVector2D::Distance(Candidate, FVector2D(Camera.LocationCm)) < 800.0f)
                {
                    return true;
                }
            }
            for (const FDiscGolfSpectatorBoundaryDefinition& Boundary : Definition.SpectatorBoundaries)
            {
                const FVector Local = Boundary.Rotation.UnrotateVector(
                    FVector(Candidate.X, Candidate.Y, Boundary.LocationCm.Z) - Boundary.LocationCm);
                if (FMath::Abs(Local.X) <= Boundary.ExtentCm.X + 250.0f
                    && FMath::Abs(Local.Y) <= Boundary.ExtentCm.Y + 250.0f)
                {
                    return true;
                }
            }
            for (const FDiscGolfCollisionFixtureDefinition& Fixture : Definition.CollisionFixtures)
            {
                const FVector Local = Fixture.Rotation.UnrotateVector(
                    FVector(Candidate.X, Candidate.Y, Fixture.LocationCm.Z) - Fixture.LocationCm);
                const FVector Extent = Fixture.Scale.GetAbs() * BasicShapeHalfExtentCm;
                if (FMath::Abs(Local.X) <= Extent.X + 260.0f
                    && FMath::Abs(Local.Y) <= Extent.Y + 260.0f)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void AddTrailWearPatch(
        const FTerrainPath& Path,
        float AlongAlpha,
        float LateralOffsetCm,
        float HalfLengthCm,
        float HalfWidthCm,
        const TArray<FTerrainPath>& CoursePaths,
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
        float Phase,
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FColor>& Colors,
        TArray<FProcMeshTangent>& Tangents)
    {
        FVector Center;
        FVector2D Direction;
        if (!SamplePolyline(Path.Points, AlongAlpha, Center, Direction)) return;
        const FVector2D Side(-Direction.Y, Direction.X);
        Center += FVector(Side.X * LateralOffsetCm, Side.Y * LateralOffsetCm, 0.0f);
        Center.Z = CourseHeightAt(FVector2D(Center), CoursePaths, Definitions, Phase) + 3.5f;
        const int32 First = Vertices.Num();
        Vertices.Add(Center);
        Normals.Add(CourseNormalAt(FVector2D(Center), CoursePaths, Definitions, Phase));
        Colors.Add(FColor::White);
        Tangents.Add(FProcMeshTangent(FVector(Direction.X, Direction.Y, 0.0f), false));
        UVs.Add(FVector2D(0.5f, 0.5f));
        constexpr int32 EdgeCount = 8;
        for (int32 EdgeIndex = 0; EdgeIndex < EdgeCount; ++EdgeIndex)
        {
            const float Angle = 2.0f * PI * static_cast<float>(EdgeIndex) / EdgeCount;
            const float PatchPhase = AlongAlpha * 17.0f + LateralOffsetCm * 0.013f;
            const float Irregularity = 0.76f
                + 0.17f * FMath::Sin(Angle * 3.0f + PatchPhase)
                + 0.09f * FMath::Sin(Angle * 7.0f - PatchPhase * 0.61f);
            const FVector2D Offset = Direction * (FMath::Cos(Angle) * HalfLengthCm * Irregularity)
                + Side * (FMath::Sin(Angle) * HalfWidthCm * Irregularity);
            FVector Edge(Center.X + Offset.X, Center.Y + Offset.Y, 0.0f);
            Edge.Z = CourseHeightAt(FVector2D(Edge), CoursePaths, Definitions, Phase) + 3.5f;
            Vertices.Add(Edge);
            Normals.Add(CourseNormalAt(FVector2D(Edge), CoursePaths, Definitions, Phase));
            Colors.Add(FColor::White);
            Tangents.Add(FProcMeshTangent(FVector(Direction.X, Direction.Y, 0.0f), false));
            UVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos(Angle),
                0.5f + 0.5f * FMath::Sin(Angle)));
        }
        for (int32 EdgeIndex = 0; EdgeIndex < EdgeCount; ++EdgeIndex)
        {
            Triangles.Append({ First, First + 1 + EdgeIndex,
                First + 1 + (EdgeIndex + 1) % EdgeCount });
        }
    }

    void AddGrassPlane(
        const FVector& Base,
        const FVector2D& Direction,
        float Width,
        float Height,
        float Lean,
        const FColor& Color,
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FColor>& Colors,
        TArray<FProcMeshTangent>& Tangents)
    {
        const int32 First = Vertices.Num();
        const FVector Side(Direction.X * Width * 0.5f, Direction.Y * Width * 0.5f, 0.0f);
        const FVector LeanVector(-Direction.Y * Lean, Direction.X * Lean, 0.0f);
        const FVector Top = Base + FVector(0.0f, 0.0f, Height) + LeanVector;
        Vertices.Add(Base - Side);
        Vertices.Add(Base + Side);
        Vertices.Add(Top - Side * 0.18f);
        Vertices.Add(Top + Side * 0.18f);
        Triangles.Append({ First, First + 2, First + 1, First + 1, First + 2, First + 3 });
        const FVector Normal(-Direction.Y, Direction.X, 0.12f);
        const FVector SafeNormal = Normal.GetSafeNormal();
        const FVector Tangent(Direction.X, Direction.Y, 0.0f);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Normals.Add(SafeNormal);
            Colors.Add(Color);
            Tangents.Add(FProcMeshTangent(Tangent, false));
        }
        UVs.Append({ FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f),
            FVector2D(0.42f, 0.0f), FVector2D(0.58f, 0.0f) });
    }

    void AddBlendedRibbon(
        const FTerrainPath& Path,
        int32 SegmentCount,
        float WidthScale,
        float TextureWorldScaleCm,
        float HeightOffsetCm,
        const TArray<FTerrainPath>& CoursePaths,
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
        float Phase,
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FColor>& Colors,
        TArray<FProcMeshTangent>& Tangents)
    {
        constexpr int32 RailCount = 5;
        const float RailFactors[RailCount] = { -1.0f, -0.76f, 0.0f, 0.76f, 1.0f };
        const uint8 BlendAlpha[RailCount] = { 0, 205, 255, 205, 0 };
        const int32 FirstVertex = Vertices.Num();
        for (int32 SegmentIndex = 0; SegmentIndex <= SegmentCount; ++SegmentIndex)
        {
            FVector Center;
            FVector2D Direction;
            const float AlongAlpha = static_cast<float>(SegmentIndex) / SegmentCount;
            if (!SamplePolyline(Path.Points, AlongAlpha, Center, Direction)) continue;
            const FVector2D Side(-Direction.Y, Direction.X);
            const float HalfWidth = Path.HalfWidthCm * WidthScale;
            Center.Z = CourseHeightAt(FVector2D(Center), CoursePaths, Definitions, Phase)
                + HeightOffsetCm;
            const FProcMeshTangent Tangent(FVector(Direction.X, Direction.Y, 0.0f), false);
            for (int32 RailIndex = 0; RailIndex < RailCount; ++RailIndex)
            {
                const FVector Vertex = Center + FVector(
                    Side.X * HalfWidth * RailFactors[RailIndex],
                    Side.Y * HalfWidth * RailFactors[RailIndex], 0.0f);
                Vertices.Add(Vertex);
                const float SafeWorldScale = FMath::Max(TextureWorldScaleCm, 1.0f);
                UVs.Add(FVector2D(Vertex.X / SafeWorldScale, Vertex.Y / SafeWorldScale));
                const uint8 Variation = GroundMacroVariation(FVector2D(Vertex), Phase);
                Colors.Add(FColor(Variation, Variation, Variation, BlendAlpha[RailIndex]));
                Normals.Add(FVector::UpVector);
                Tangents.Add(Tangent);
            }
        }
        for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
        {
            for (int32 RailIndex = 0; RailIndex < RailCount - 1; ++RailIndex)
            {
                const int32 A = FirstVertex + SegmentIndex * RailCount + RailIndex;
                const int32 B = A + 1;
                const int32 C = A + RailCount;
                const int32 D = C + 1;
                Triangles.Append({ A, C, B, B, C, D });
            }
        }
    }

    void AddLitterQuad(
        const FVector& Center,
        float HalfWidth,
        float HalfLength,
        float YawRadians,
        const FColor& Color,
        TArray<FVector>& Vertices,
        TArray<int32>& Triangles,
        TArray<FVector>& Normals,
        TArray<FVector2D>& UVs,
        TArray<FColor>& Colors,
        TArray<FProcMeshTangent>& Tangents)
    {
        const FVector2D Forward(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
        const FVector2D Side(-Forward.Y, Forward.X);
        const FVector Forward3(Forward.X * HalfLength, Forward.Y * HalfLength, 0.0f);
        const FVector Side3(Side.X * HalfWidth, Side.Y * HalfWidth, 0.0f);
        const int32 First = Vertices.Num();
        Vertices.Append({ Center - Forward3 - Side3, Center - Forward3 + Side3,
            Center + Forward3 + Side3, Center + Forward3 - Side3 });
        Triangles.Append({ First, First + 2, First + 1, First, First + 3, First + 2 });
        UVs.Append({ FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f),
            FVector2D(1.0f, 0.0f), FVector2D(0.0f, 0.0f) });
        const FProcMeshTangent Tangent(FVector(Forward.X, Forward.Y, 0.0f), false);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Normals.Add(FVector::UpVector);
            Colors.Add(Color);
            Tangents.Add(Tangent);
        }
    }

    void AddVerticalCardInstance(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FVector& Base,
        const FVector& GroundNormal,
        float YawRadians,
        float WidthCm,
        float HeightCm,
        float LeanCm)
    {
        if (!Component) return;
        FVector Forward(FMath::Cos(YawRadians), FMath::Sin(YawRadians), 0.0f);
        Forward = FVector::VectorPlaneProject(Forward, GroundNormal)
            .GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
        const FVector Side = FVector::CrossProduct(GroundNormal, Forward)
            .GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
        const FVector BladeVector = GroundNormal * HeightCm + Forward * LeanCm;
        const FVector BladeUp = BladeVector.GetSafeNormal(UE_SMALL_NUMBER, GroundNormal);
        // Engine Plane lies in local XY with its long source UV direction on local Y.
        // Keep local X as card width and local Y as blade height.
        const FQuat Rotation = FRotationMatrix::MakeFromXY(Side, BladeUp).ToQuat();
        const FVector Location = Base + BladeVector * 0.5f;
        const FVector Scale(WidthCm / 100.0f, BladeVector.Size() / 100.0f, 1.0f);
        Component->AddInstance(FTransform(Rotation, Location, Scale), false);
    }

    void AddGroundCardInstance(
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FVector& Center,
        const FVector& GroundNormal,
        float YawRadians,
        float WidthCm,
        float LengthCm)
    {
        if (!Component) return;
        FVector Forward(FMath::Cos(YawRadians), FMath::Sin(YawRadians), 0.0f);
        Forward = FVector::VectorPlaneProject(Forward, GroundNormal)
            .GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
        const FVector Side = FVector::CrossProduct(GroundNormal, Forward)
            .GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
        const FQuat Rotation = FRotationMatrix::MakeFromXY(Side, Forward).ToQuat();
        Component->AddInstance(FTransform(Rotation, Center,
            FVector(WidthCm / 100.0f, LengthCm / 100.0f, 1.0f)), false);
    }

    void AddFittedMeshInstance(
        UHierarchicalInstancedStaticMeshComponent* Component,
        UStaticMesh* Mesh,
        const FVector& Center,
        const FQuat& Rotation,
        const FVector& DesiredFullSizeCm)
    {
        if (!Component || !Mesh) return;
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FVector MeshSize(
            FMath::Max(Bounds.BoxExtent.X * 2.0f, 1.0f),
            FMath::Max(Bounds.BoxExtent.Y * 2.0f, 1.0f),
            FMath::Max(Bounds.BoxExtent.Z * 2.0f, 1.0f));
        const FVector Scale(DesiredFullSizeCm.X / MeshSize.X,
            DesiredFullSizeCm.Y / MeshSize.Y, DesiredFullSizeCm.Z / MeshSize.Z);
        const FVector PivotOffset = Rotation.RotateVector(Bounds.Origin * Scale);
        Component->AddInstance(FTransform(Rotation, Center - PivotOffset, Scale), false);
    }
}

ADiscGolfTerrainPresentationActor::ADiscGolfTerrainPresentationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.AddUnique(TEXT("Presentation.CourseTerrain"));

    TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
    SetRootComponent(TerrainMesh);
    GrassMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GrassMesh"));
    GrassMesh->SetupAttachment(TerrainMesh);

    FineGrassInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("FineGrassInstances"));
    SedgeGrassInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("SedgeGrassInstances"));
    FernGrassInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("FernGrassInstances"));
    LeafLitterInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("LeafLitterInstances"));
    NeedleLitterInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("NeedleLitterInstances"));
    ShoreRockInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("ShoreRockInstances"));
    ShoreReedInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("ShoreReedInstances"));
    ShoreDeadfallInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("ShoreDeadfallInstances"));
    for (UHierarchicalInstancedStaticMeshComponent* Component : {
        FineGrassInstances.Get(), SedgeGrassInstances.Get(), FernGrassInstances.Get(),
        LeafLitterInstances.Get(), NeedleLitterInstances.Get(), ShoreRockInstances.Get(),
        ShoreReedInstances.Get(), ShoreDeadfallInstances.Get() })
    {
        Component->SetupAttachment(TerrainMesh);
    }

    for (UPrimitiveComponent* Component : { static_cast<UPrimitiveComponent*>(TerrainMesh.Get()),
        static_cast<UPrimitiveComponent*>(GrassMesh.Get()),
        static_cast<UPrimitiveComponent*>(FineGrassInstances.Get()),
        static_cast<UPrimitiveComponent*>(SedgeGrassInstances.Get()),
        static_cast<UPrimitiveComponent*>(FernGrassInstances.Get()),
        static_cast<UPrimitiveComponent*>(LeafLitterInstances.Get()),
        static_cast<UPrimitiveComponent*>(NeedleLitterInstances.Get()),
        static_cast<UPrimitiveComponent*>(ShoreRockInstances.Get()),
        static_cast<UPrimitiveComponent*>(ShoreReedInstances.Get()),
        static_cast<UPrimitiveComponent*>(ShoreDeadfallInstances.Get()) })
    {
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }
    // The continuous base mesh is the visible and physical ground authority.
    // Its geometry is deterministic across quality tiers; every decorative
    // section/component remains collision-free.
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TerrainMesh->SetCollisionObjectType(ECC_WorldStatic);
    TerrainMesh->SetCollisionResponseToAllChannels(ECR_Block);
    TerrainMesh->bUseComplexAsSimpleCollision = true;
    TerrainMesh->SetCastShadow(false);
    GrassMesh->SetCastShadow(false);
    FineGrassInstances->SetCastShadow(false);
    SedgeGrassInstances->SetCastShadow(false);
    FernGrassInstances->SetCastShadow(false);
    LeafLitterInstances->SetCastShadow(false);
    NeedleLitterInstances->SetCastShadow(false);
    ShoreRockInstances->SetCastShadow(true);
    ShoreReedInstances->SetCastShadow(false);
    ShoreDeadfallInstances->SetCastShadow(true);
}

void ADiscGolfTerrainPresentationActor::Configure(
    const FDiscGolfHoleBlockoutDefinition& Definition,
    int32 TerrainSeed)
{
    TArray<FDiscGolfHoleBlockoutDefinition> Definitions = { Definition };
    TArray<int32> Seeds = { TerrainSeed };
    ConfigureCourse(Definitions, Seeds, 0.0f, 1.0f);
}

bool ADiscGolfTerrainPresentationActor::ConfigureCourse(
    const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
    const TArray<int32>& TerrainSeeds,
    float GrassDensityScale,
    float CullDistanceScale)
{
    TriangleCount = 0;
    ConnectorTriangleCount = 0;
    FairwayTriangleCount = 0;
    ShorelineTriangleCount = 0;
    TrailWearPatchCount = 0;
    TrailWearTriangleCount = 0;
    GrassBladeClusterCount = 0;
    GrassTriangleCount = 0;
    GrassCardInstanceCount = 0;
    GrassSpeciesVariantCount = 0;
    LitterClusterCount = 0;
    LitterTriangleCount = 0;
    LitterVariantCount = 0;
    GrassClustersByHole.Reset();
    FernClustersByHole.Reset();
    LitterClustersByHole.Reset();
    GroundCoverCullStartDistanceCm = 0;
    GroundCoverCullEndDistanceCm = 0;
    MaxGroundCoverSlopeDegrees = 0.0f;
    MinGroundMacroVariation = 255;
    MaxGroundMacroVariation = 0;
    MaxShorelineErosionOffsetCm = 0.0f;
    ShoreRockInstanceCount = 0;
    ShoreReedClusterCount = 0;
    ShoreReedStemInstanceCount = 0;
    ShoreDeadfallInstanceCount = 0;
    ShoreAccentVariantCount = 0;
    MinShorelineAccentRouteClearanceCm = 0.0f;
    CourseHoleCount = 0;
    bReady = false;
    if (!TerrainMesh || !GrassMesh || Definitions.IsEmpty()
        || TerrainSeeds.Num() != Definitions.Num())
    {
        return false;
    }

    TerrainMesh->ClearAllMeshSections();
    GrassMesh->ClearAllMeshSections();
    for (UHierarchicalInstancedStaticMeshComponent* Component : {
        FineGrassInstances.Get(), SedgeGrassInstances.Get(), FernGrassInstances.Get(),
        LeafLitterInstances.Get(), NeedleLitterInstances.Get(), ShoreRockInstances.Get(),
        ShoreReedInstances.Get(), ShoreDeadfallInstances.Get() })
    {
        Component->ClearInstances();
    }
    for (UPrimitiveComponent* Component : { static_cast<UPrimitiveComponent*>(TerrainMesh.Get()),
        static_cast<UPrimitiveComponent*>(GrassMesh.Get()),
        static_cast<UPrimitiveComponent*>(FineGrassInstances.Get()),
        static_cast<UPrimitiveComponent*>(SedgeGrassInstances.Get()),
        static_cast<UPrimitiveComponent*>(FernGrassInstances.Get()),
        static_cast<UPrimitiveComponent*>(LeafLitterInstances.Get()),
        static_cast<UPrimitiveComponent*>(NeedleLitterInstances.Get()),
        static_cast<UPrimitiveComponent*>(ShoreRockInstances.Get()),
        static_cast<UPrimitiveComponent*>(ShoreReedInstances.Get()),
        static_cast<UPrimitiveComponent*>(ShoreDeadfallInstances.Get()) })
    {
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }
    TerrainMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TerrainMesh->SetCollisionObjectType(ECC_WorldStatic);
    TerrainMesh->SetCollisionResponseToAllChannels(ECR_Block);
    TerrainMesh->bUseComplexAsSimpleCollision = true;

    const TArray<FTerrainPath> Paths = BuildCoursePaths(Definitions);
    if (Paths.IsEmpty()) return false;

    FBox2D Bounds(EForceInit::ForceInit);
    for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
    {
        Bounds += FVector2D(Definition.TeeLocationCm);
        Bounds += FVector2D(Definition.BasketLocationCm);
        for (const FDiscGolfBlockoutSurfaceDefinition& Surface : Definition.Surfaces)
        {
            const FVector2D Extent(BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.X),
                BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.Y));
            Bounds += FVector2D(Surface.LocationCm) - Extent;
            Bounds += FVector2D(Surface.LocationCm) + Extent;
        }
        for (const FDiscGolfShotRouteDefinition& Route : Definition.ShotRoutes)
        {
            for (const FVector& Point : Route.WaypointsCm) Bounds += FVector2D(Point);
        }
    }
    if (!Bounds.bIsValid) return false;
    Bounds.Min -= FVector2D(CourseMarginCm);
    Bounds.Max += FVector2D(CourseMarginCm);
    const FVector2D Span = Bounds.GetSize();
    const int32 XSegments = FMath::Clamp(FMath::CeilToInt(Span.X / GridSpacingCm), 48, 224);
    const int32 YSegments = FMath::Clamp(FMath::CeilToInt(Span.Y / GridSpacingCm), 32, 144);
    const int32 RowSize = XSegments + 1;

    int32 CombinedSeed = 0;
    for (int32 Seed : TerrainSeeds) CombinedSeed = HashCombine(CombinedSeed, GetTypeHash(Seed));
    const float Phase = FMath::Fmod(FMath::Abs(static_cast<float>(CombinedSeed))
        * 0.0000174532925f, 2.0f * PI);

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    const int32 VertexCount = (XSegments + 1) * (YSegments + 1);
    Vertices.Reserve(VertexCount);
    UVs.Reserve(VertexCount);
    Colors.Reserve(VertexCount);

    for (int32 YIndex = 0; YIndex <= YSegments; ++YIndex)
    {
        const float V = static_cast<float>(YIndex) / YSegments;
        for (int32 XIndex = 0; XIndex <= XSegments; ++XIndex)
        {
            const float U = static_cast<float>(XIndex) / XSegments;
            const FVector2D Point(FMath::Lerp(Bounds.Min.X, Bounds.Max.X, U),
                FMath::Lerp(Bounds.Min.Y, Bounds.Max.Y, V));
            Vertices.Add(FVector(Point.X, Point.Y,
                CourseHeightAt(Point, Paths, Definitions, Phase)));
            // Give every course surface one deterministic physical UV authority.
            // A 2 m repeat remains stable regardless of the full-property bounds
            // and cannot degrade into a single stretched texture in a cooked build.
            UVs.Add(FVector2D(
                Point.X / GroundTextureWorldScaleCm,
                Point.Y / GroundTextureWorldScaleCm));
            const uint8 Variation = GroundMacroVariation(Point, Phase);
            MinGroundMacroVariation = FMath::Min(MinGroundMacroVariation,
                static_cast<int32>(Variation));
            MaxGroundMacroVariation = FMath::Max(MaxGroundMacroVariation,
                static_cast<int32>(Variation));
            Colors.Add(FColor(Variation, Variation, Variation, 255));
        }
    }

    for (int32 YIndex = 0; YIndex < YSegments; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex < XSegments; ++XIndex)
        {
            const int32 A = YIndex * RowSize + XIndex;
            const int32 B = A + 1;
            const int32 C = A + RowSize;
            const int32 D = C + 1;
            // ProceduralMesh uses clockwise winding for the visible face. Keep the geometric
            // normals upward while winding the course bed consistently with its ribbon sections.
            Triangles.Append({ A, D, B, A, C, D });
        }
    }

    Normals.SetNumUninitialized(Vertices.Num());
    Tangents.SetNumUninitialized(Vertices.Num());
    for (int32 YIndex = 0; YIndex <= YSegments; ++YIndex)
    {
        for (int32 XIndex = 0; XIndex <= XSegments; ++XIndex)
        {
            const int32 Index = YIndex * RowSize + XIndex;
            const int32 PrevX = YIndex * RowSize + FMath::Max(XIndex - 1, 0);
            const int32 NextX = YIndex * RowSize + FMath::Min(XIndex + 1, XSegments);
            const int32 PrevY = FMath::Max(YIndex - 1, 0) * RowSize + XIndex;
            const int32 NextY = FMath::Min(YIndex + 1, YSegments) * RowSize + XIndex;
            const FVector XTangent = (Vertices[NextX] - Vertices[PrevX]).GetSafeNormal();
            const FVector YTangent = (Vertices[NextY] - Vertices[PrevY]).GetSafeNormal();
            Normals[Index] = FVector::CrossProduct(XTangent, YTangent)
                .GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
            Tangents[Index] = FProcMeshTangent(XTangent, false);
        }
    }

    // Defer collision until every decorative section has been installed. Each
    // CreateMeshSection invalidates ProceduralMesh collision, so enabling the
    // 10k+ triangle base here would synchronously recook it for every overlay.
    TerrainMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
    TriangleCount = Triangles.Num() / 3;
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeForestFloor.MI_PineRidgeForestFloor")))
    {
        if (UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, this))
        {
            // UV0 above is the sole terrain sampling authority. Explicitly
            // disable the material instance's alternate world-position branch
            // so cook-time instance overrides cannot change the texel scale.
            Dynamic->SetScalarParameterValue(TEXT("UseWorldUV"), 0.0f);
            Dynamic->SetScalarParameterValue(TEXT("DetailStrength"), 0.0f);
            Dynamic->SetScalarParameterValue(TEXT("AlbedoContrastPower"), 0.88f);
            Dynamic->SetScalarParameterValue(TEXT("MacroVariationRange"), 0.07f);
            Dynamic->SetScalarParameterValue(TEXT("MacroVariationMinimum"), 0.88f);
            Dynamic->SetVectorParameterValue(TEXT("BaseColorTint"),
                FLinearColor(0.24f, 0.30f, 0.20f, 1.0f));
            Dynamic->SetScalarParameterValue(TEXT("UTiling"), 1.0f);
            Dynamic->SetScalarParameterValue(TEXT("VTiling"), 1.0f);
            TerrainMesh->SetMaterial(0, Dynamic);
        }
    }

    TArray<FVector> TrailVertices;
    TArray<int32> TrailTriangles;
    TArray<FVector> TrailNormals;
    TArray<FVector2D> TrailUVs;
    TArray<FColor> TrailColors;
    TArray<FProcMeshTangent> TrailTangents;
    constexpr int32 ConnectorSegments = 32;
    for (const FTerrainPath& Path : Paths)
    {
        if (!Path.bConnector) continue;
        AddBlendedRibbon(Path, ConnectorSegments, 1.35f, GroundTextureWorldScaleCm, 2.5f,
            Paths, Definitions, Phase, TrailVertices, TrailTriangles, TrailNormals,
            TrailUVs, TrailColors, TrailTangents);
    }
    if (!TrailTriangles.IsEmpty())
    {
        TerrainMesh->CreateMeshSection(1, TrailVertices, TrailTriangles, TrailNormals,
            TrailUVs, TrailColors, TrailTangents, false);
        ConnectorTriangleCount = TrailTriangles.Num() / 3;
        if (UMaterialInterface* PathMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeTrailBlend.M_PineRidgeTrailBlend")))
        {
            TerrainMesh->SetMaterial(1, PathMaterial);
        }
    }

    TArray<FVector> FairwayVertices;
    TArray<int32> FairwayTriangles;
    TArray<FVector> FairwayNormals;
    TArray<FVector2D> FairwayUVs;
    TArray<FColor> FairwayColors;
    TArray<FProcMeshTangent> FairwayTangents;
    constexpr int32 FairwaySegments = 48;
    for (const FTerrainPath& Path : Paths)
    {
        if (Path.bConnector) continue;
        AddBlendedRibbon(Path, FairwaySegments, 1.12f, GroundTextureWorldScaleCm, 1.8f,
            Paths, Definitions, Phase, FairwayVertices, FairwayTriangles, FairwayNormals,
            FairwayUVs, FairwayColors, FairwayTangents);
    }
    if (!FairwayTriangles.IsEmpty())
    {
        TerrainMesh->CreateMeshSection(2, FairwayVertices, FairwayTriangles, FairwayNormals,
            FairwayUVs, FairwayColors, FairwayTangents, false);
        FairwayTriangleCount = FairwayTriangles.Num() / 3;
        if (UMaterialInterface* FairwayMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeFairwayBlend.M_PineRidgeFairwayBlend")))
        {
            TerrainMesh->SetMaterial(2, FairwayMaterial);
        }
    }

    TArray<FVector> WearVertices;
    TArray<int32> WearTriangles;
    TArray<FVector> WearNormals;
    TArray<FVector2D> WearUVs;
    TArray<FColor> WearColors;
    TArray<FProcMeshTangent> WearTangents;
    FRandomStream WearRandom(CombinedSeed ^ 0x7411a1b9);
    constexpr int32 PatchesPerConnector = 18;
    for (const FTerrainPath& Path : Paths)
    {
        if (!Path.bConnector) continue;
        for (int32 PatchIndex = 0; PatchIndex < PatchesPerConnector; ++PatchIndex)
        {
            const float Along = (static_cast<float>(PatchIndex) + WearRandom.FRandRange(0.18f, 0.82f))
                / PatchesPerConnector;
            AddTrailWearPatch(Path, Along, WearRandom.FRandRange(-105.0f, 105.0f),
                WearRandom.FRandRange(115.0f, 235.0f), WearRandom.FRandRange(42.0f, 105.0f),
                Paths, Definitions, Phase, WearVertices, WearTriangles, WearNormals,
                WearUVs, WearColors, WearTangents);
            ++TrailWearPatchCount;
        }
    }
    if (!WearTriangles.IsEmpty())
    {
        TerrainMesh->CreateMeshSection(4, WearVertices, WearTriangles, WearNormals,
            WearUVs, WearColors, WearTangents, false);
        TrailWearTriangleCount = WearTriangles.Num() / 3;
        if (UMaterialInterface* WearMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeTrailWear.MI_PineRidgeTrailWear")))
        {
            TerrainMesh->SetMaterial(4, WearMaterial);
        }
    }

    TArray<FVector> ShoreVertices;
    TArray<int32> ShoreTriangles;
    TArray<FVector> ShoreNormals;
    TArray<FVector2D> ShoreUVs;
    TArray<FColor> ShoreColors;
    TArray<FProcMeshTangent> ShoreTangents;
    constexpr int32 ShoreSegments = 96;
    constexpr int32 ShoreRails = 4;
    const float ShoreRadiusFactors[ShoreRails] = { 0.92f, 1.01f, 1.14f, 1.34f };
    const uint8 ShoreBlendAlpha[ShoreRails] = { 0, 255, 184, 0 };
    const FDiscGolfBlockoutSurfaceDefinition* LakeSurface = nullptr;
    for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
    {
        LakeSurface = Definition.Surfaces.FindByPredicate(
            [](const FDiscGolfBlockoutSurfaceDefinition& Surface)
            {
                return IsLakeSurface(Surface);
            });
        if (LakeSurface) break;
    }
    if (LakeSurface)
    {
        const float RadiusX = BasicShapeHalfExtentCm * FMath::Abs(LakeSurface->Scale.X);
        const float RadiusY = BasicShapeHalfExtentCm * FMath::Abs(LakeSurface->Scale.Y);
        const float WaterHeight = SurfaceTop(*LakeSurface);
        for (int32 SegmentIndex = 0; SegmentIndex < ShoreSegments; ++SegmentIndex)
        {
            const float Alpha = static_cast<float>(SegmentIndex) / ShoreSegments;
            const float Angle = Alpha * 2.0f * PI;
            const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
            const FVector2D Tangent2D(-Direction.Y, Direction.X);
            for (int32 RailIndex = 0; RailIndex < ShoreRails; ++RailIndex)
            {
                const float ErosionStrength = static_cast<float>(RailIndex) / (ShoreRails - 1);
                const float ErosionNoise = ErosionStrength * (
                    0.038f * FMath::Sin(Angle * 5.0f + Phase)
                    + 0.019f * FMath::Sin(Angle * 11.0f - Phase * 1.37f));
                const float RadiusFactor = ShoreRadiusFactors[RailIndex] + ErosionNoise;
                MaxShorelineErosionOffsetCm = FMath::Max(MaxShorelineErosionOffsetCm,
                    FMath::Abs(ErosionNoise) * FMath::Max(RadiusX, RadiusY));
                const FVector Local(Direction.X * RadiusX * RadiusFactor,
                    Direction.Y * RadiusY * RadiusFactor, 0.0f);
                FVector World = LakeSurface->LocationCm + LakeSurface->Rotation.RotateVector(Local);
                const float GroundHeight = CourseHeightAt(FVector2D(World), Paths, Definitions, Phase);
                if (RailIndex == 0) World.Z = WaterHeight - 4.0f;
                else if (RailIndex == 1) World.Z = WaterHeight + 1.5f;
                else if (RailIndex == 2) World.Z = FMath::Lerp(WaterHeight + 4.0f, GroundHeight + 3.0f, 0.62f);
                else World.Z = GroundHeight + 2.0f;
                ShoreVertices.Add(World);
                ShoreUVs.Add(FVector2D(World.X / GroundTextureWorldScaleCm,
                    World.Y / GroundTextureWorldScaleCm));
                const uint8 Variation = GroundMacroVariation(FVector2D(World), Phase);
                ShoreColors.Add(FColor(Variation, Variation, Variation,
                    ShoreBlendAlpha[RailIndex]));
                ShoreNormals.Add(FVector::UpVector);
                ShoreTangents.Add(FProcMeshTangent(
                    LakeSurface->Rotation.RotateVector(FVector(Tangent2D.X, Tangent2D.Y, 0.0f)), false));
            }
        }
        for (int32 SegmentIndex = 0; SegmentIndex < ShoreSegments; ++SegmentIndex)
        {
            const int32 NextSegment = (SegmentIndex + 1) % ShoreSegments;
            for (int32 RailIndex = 0; RailIndex < ShoreRails - 1; ++RailIndex)
            {
                const int32 A = SegmentIndex * ShoreRails + RailIndex;
                const int32 B = A + 1;
                const int32 C = NextSegment * ShoreRails + RailIndex;
                const int32 D = C + 1;
                ShoreTriangles.Append({ A, C, B, B, C, D });
            }
        }
        TerrainMesh->CreateMeshSection(3, ShoreVertices, ShoreTriangles, ShoreNormals,
            ShoreUVs, ShoreColors, ShoreTangents, false);
        ShorelineTriangleCount = ShoreTriangles.Num() / 3;
        if (UMaterialInterface* ShoreMaterial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeTrailBlend.M_PineRidgeTrailBlend")))
        {
            TerrainMesh->SetMaterial(3, ShoreMaterial);
        }
    }

    UStaticMesh* BoulderMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Fixtures/PolyHaven/Boulder01/boulder_01_1k.boulder_01_1k"));
    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UMaterialInterface* BoulderMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_Boulder01.MI_Boulder01"));
    UMaterialInterface* ReedMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeShoreReeds.MI_PineRidgeShoreReeds"));
    UMaterialInterface* DeadfallMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_FirSaplingBranches.MI_FirSaplingBranches"));
    UStaticMesh* GroundCoverCard = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Plane.Plane"));
    const bool bShoreAssetsReady = !LakeSurface || (BoulderMesh && CylinderMesh
        && BoulderMaterial && ReedMaterial && DeadfallMaterial);
    if (LakeSurface && bShoreAssetsReady)
    {
        ShoreRockInstances->SetStaticMesh(BoulderMesh);
        ShoreRockInstances->SetMaterial(0, BoulderMaterial);
        ShoreRockInstances->SetCullDistances(18000, 42000);
        ShoreReedInstances->SetStaticMesh(CylinderMesh);
        ShoreReedInstances->SetMaterial(0, ReedMaterial);
        ShoreReedInstances->SetCullDistances(12000, 32000);
        ShoreDeadfallInstances->SetStaticMesh(CylinderMesh);
        ShoreDeadfallInstances->SetMaterial(0, DeadfallMaterial);
        ShoreDeadfallInstances->SetCullDistances(18000, 42000);

        constexpr int32 RockTarget = 42;
        constexpr int32 ReedTarget = 96;
        constexpr int32 DeadfallTarget = 14;
        constexpr float MinimumRouteClearanceCm = 1050.0f;
        const float RadiusX = BasicShapeHalfExtentCm * FMath::Abs(LakeSurface->Scale.X);
        const float RadiusY = BasicShapeHalfExtentCm * FMath::Abs(LakeSurface->Scale.Y);
        FRandomStream ShoreRandom(CombinedSeed ^ 0x18d3a5c7);
        auto SampleShore = [&](float MinRadiusFactor, float MaxRadiusFactor,
            FVector2D& OutCandidate, FVector2D& OutTangent, FVector& OutNormal)
        {
            const float Angle = ShoreRandom.FRandRange(-PI, PI);
            const float RadiusFactor = ShoreRandom.FRandRange(MinRadiusFactor, MaxRadiusFactor);
            const FVector Local(FMath::Cos(Angle) * RadiusX * RadiusFactor,
                FMath::Sin(Angle) * RadiusY * RadiusFactor, 0.0f);
            const FVector World = LakeSurface->LocationCm + LakeSurface->Rotation.RotateVector(Local);
            OutCandidate = FVector2D(World);
            const FVector LocalTangent(-FMath::Sin(Angle) * RadiusX,
                FMath::Cos(Angle) * RadiusY, 0.0f);
            OutTangent = FVector2D(LakeSurface->Rotation.RotateVector(LocalTangent)).GetSafeNormal();
            OutNormal = CourseNormalAt(OutCandidate, Paths, Definitions, Phase);
            return !IsProtectedShoreCandidate(OutCandidate, Definitions, MinimumRouteClearanceCm)
                && OutNormal.Z >= FMath::Cos(FMath::DegreesToRadians(34.0f));
        };
        auto RecordRouteClearance = [&](const FVector2D& Candidate)
        {
            const float Clearance = NearestAuthoredRouteDistanceCm(Candidate, Definitions);
            MinShorelineAccentRouteClearanceCm = MinShorelineAccentRouteClearanceCm <= 0.0f
                ? Clearance : FMath::Min(MinShorelineAccentRouteClearanceCm, Clearance);
        };

        for (int32 Attempt = 0; Attempt < RockTarget * 18
            && ShoreRockInstanceCount < RockTarget; ++Attempt)
        {
            FVector2D Candidate;
            FVector2D Tangent;
            FVector GroundNormal;
            if (!SampleShore(1.04f, 1.28f, Candidate, Tangent, GroundNormal)) continue;
            const float Height = ShoreRandom.FRandRange(35.0f, 115.0f);
            const FVector Center(Candidate.X, Candidate.Y,
                CourseHeightAt(Candidate, Paths, Definitions, Phase) + Height * 0.35f);
            const FVector Forward = FVector::VectorPlaneProject(
                FVector(Tangent.X, Tangent.Y, 0.0f), GroundNormal)
                .GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
            const FQuat Rotation = FRotationMatrix::MakeFromXZ(Forward, GroundNormal).ToQuat();
            AddFittedMeshInstance(ShoreRockInstances, BoulderMesh, Center, Rotation,
                FVector(ShoreRandom.FRandRange(75.0f, 185.0f),
                    ShoreRandom.FRandRange(60.0f, 145.0f), Height));
            ++ShoreRockInstanceCount;
            RecordRouteClearance(Candidate);
        }
        for (int32 Attempt = 0; Attempt < ReedTarget * 18
            && ShoreReedClusterCount < ReedTarget; ++Attempt)
        {
            FVector2D Candidate;
            FVector2D Tangent;
            FVector GroundNormal;
            if (!SampleShore(1.02f, 1.18f, Candidate, Tangent, GroundNormal)) continue;
            const float Height = ShoreRandom.FRandRange(62.0f, 138.0f);
            const FVector Base(Candidate.X, Candidate.Y,
                CourseHeightAt(Candidate, Paths, Definitions, Phase) + 1.5f);
            const float Yaw = FMath::Atan2(Tangent.Y, Tangent.X) + ShoreRandom.FRandRange(-0.42f, 0.42f);
            FVector Forward(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0f);
            Forward = FVector::VectorPlaneProject(Forward, GroundNormal)
                .GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
            const FVector Side = FVector::CrossProduct(GroundNormal, Forward)
                .GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
            for (int32 StemIndex = 0; StemIndex < 4; ++StemIndex)
            {
                const float StemHeight = Height * ShoreRandom.FRandRange(0.72f, 1.03f);
                const float Diameter = ShoreRandom.FRandRange(1.4f, 2.8f);
                const FVector StemBase = Base + Forward * ShoreRandom.FRandRange(-18.0f, 18.0f)
                    + Side * ShoreRandom.FRandRange(-16.0f, 16.0f);
                const FVector StemDirection = (GroundNormal
                    + Forward * ShoreRandom.FRandRange(-0.10f, 0.14f)
                    + Side * ShoreRandom.FRandRange(-0.08f, 0.08f)).GetSafeNormal();
                const FQuat StemRotation = FRotationMatrix::MakeFromZ(StemDirection).ToQuat();
                AddFittedMeshInstance(ShoreReedInstances, CylinderMesh,
                    StemBase + StemDirection * StemHeight * 0.5f, StemRotation,
                    FVector(Diameter, Diameter, StemHeight));
                ++ShoreReedStemInstanceCount;
            }
            ++ShoreReedClusterCount;
            RecordRouteClearance(Candidate);
        }
        for (int32 Attempt = 0; Attempt < DeadfallTarget * 22
            && ShoreDeadfallInstanceCount < DeadfallTarget; ++Attempt)
        {
            FVector2D Candidate;
            FVector2D Tangent;
            FVector GroundNormal;
            if (!SampleShore(1.10f, 1.36f, Candidate, Tangent, GroundNormal)) continue;
            FVector Forward(Tangent.X, Tangent.Y, 0.0f);
            Forward = FVector::VectorPlaneProject(Forward, GroundNormal)
                .GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
            const FVector Side = FVector::CrossProduct(GroundNormal, Forward)
                .GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
            const FQuat Rotation = FRotationMatrix::MakeFromZX(Forward, Side).ToQuat();
            const float Length = ShoreRandom.FRandRange(220.0f, 520.0f);
            const float Diameter = ShoreRandom.FRandRange(18.0f, 42.0f);
            const FVector Center(Candidate.X, Candidate.Y,
                CourseHeightAt(Candidate, Paths, Definitions, Phase) + Diameter * 0.45f);
            AddFittedMeshInstance(ShoreDeadfallInstances, CylinderMesh, Center, Rotation,
                FVector(Diameter, Diameter, Length));
            ++ShoreDeadfallInstanceCount;
            RecordRouteClearance(Candidate);
        }
        ShoreAccentVariantCount = (ShoreRockInstanceCount > 0 ? 1 : 0)
            + (ShoreReedClusterCount > 0 ? 1 : 0)
            + (ShoreDeadfallInstanceCount > 0 ? 1 : 0);
    }

    UMaterialInterface* GrassMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeGrassBlade.M_PineRidgeGrassBlade"));
    UMaterialInterface* LitterMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeLeafLitter.M_PineRidgeLeafLitter"));
    const bool bGroundCoverAssetsReady = GroundCoverCard && GrassMaterial && LitterMaterial;
    const float SafeCullScale = FMath::Clamp(CullDistanceScale, 0.25f, 2.0f);
    GroundCoverCullStartDistanceCm = FMath::RoundToInt(9000.0f * SafeCullScale);
    GroundCoverCullEndDistanceCm = FMath::RoundToInt(24000.0f * SafeCullScale);

    UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        FineGrassInstances.Get(), SedgeGrassInstances.Get(), FernGrassInstances.Get() };
    const FLinearColor GrassTints[] = {
        FLinearColor(0.055f, 0.13f, 0.022f),
        FLinearColor(0.035f, 0.095f, 0.018f),
        FLinearColor(0.045f, 0.11f, 0.021f) };
    const float WindStrengths[] = { 4.5f, 3.5f, 2.5f };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassComponents); ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = GrassComponents[Index];
        Component->SetStaticMesh(GroundCoverCard);
        Component->SetCullDistances(GroundCoverCullStartDistanceCm,
            GroundCoverCullEndDistanceCm);
        if (GrassMaterial)
        {
            Component->SetMaterial(0, GrassMaterial);
            if (UMaterialInstanceDynamic* Dynamic = Component->CreateDynamicMaterialInstance(
                0, GrassMaterial))
            {
                Dynamic->SetVectorParameterValue(TEXT("BladeTint"), GrassTints[Index]);
                Dynamic->SetScalarParameterValue(TEXT("WindStrength"), WindStrengths[Index]);
                Dynamic->SetScalarParameterValue(TEXT("AmbientColorLift"), 0.015f);
                Dynamic->SetScalarParameterValue(TEXT("ShadeRange"), 0.18f);
                Dynamic->SetScalarParameterValue(TEXT("ShadeMinimum"), 0.68f);
            }
        }
    }

    UHierarchicalInstancedStaticMeshComponent* LitterComponents[] = {
        LeafLitterInstances.Get(), NeedleLitterInstances.Get() };
    const FLinearColor LitterTints[] = {
        FLinearColor(0.25f, 0.12f, 0.035f),
        FLinearColor(0.15f, 0.075f, 0.025f) };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(LitterComponents); ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component = LitterComponents[Index];
        Component->SetStaticMesh(GroundCoverCard);
        Component->SetCullDistances(FMath::RoundToInt(GroundCoverCullStartDistanceCm * 0.55f),
            FMath::RoundToInt(GroundCoverCullEndDistanceCm * 0.72f));
        if (LitterMaterial)
        {
            Component->SetMaterial(0, LitterMaterial);
            if (UMaterialInstanceDynamic* Dynamic = Component->CreateDynamicMaterialInstance(
                0, LitterMaterial))
            {
                Dynamic->SetVectorParameterValue(TEXT("LitterTint"), LitterTints[Index]);
            }
        }
    }

    // The current crossed-plane layer is honest synthetic validation art. Keep
    // it available to development tests, but never expose it as release foliage.
    const float RuntimeGroundCoverDensityScale = DG_RELEASE_V05_SCOPE != 0
        ? 0.0f
        : GrassDensityScale;
    const int32 GrassTarget = FMath::Clamp(
        FMath::RoundToInt(6500.0f * FMath::Clamp(
            RuntimeGroundCoverDensityScale, 0.0f, 1.5f)),
        0, 21000);
    if (GrassTarget > 0 && bGroundCoverAssetsReady)
    {
        FRandomStream Random(CombinedSeed ^ 0x5f3759df);
        const int32 MaxAttempts = GrassTarget * 12;
        for (int32 Attempt = 0;
            Attempt < MaxAttempts && GrassBladeClusterCount < GrassTarget;
            ++Attempt)
        {
            const FTerrainPath& Path = Paths[Random.RandRange(0, Paths.Num() - 1)];
            FVector Center;
            FVector2D Direction;
            if (!SamplePolyline(Path.Points, Random.FRand(), Center, Direction)) continue;
            const FVector2D Side(-Direction.Y, Direction.X);
            const float Lateral = (Random.RandRange(0, 1) == 0 ? -1.0f : 1.0f)
                * Random.FRandRange(Path.HalfWidthCm + 180.0f,
                    Path.bConnector ? 1600.0f : 4400.0f);
            const FVector2D Candidate = FVector2D(Center) + Side * Lateral
                + Direction * Random.FRandRange(-180.0f, 180.0f);
            if (IsProtectedGrassCandidate(Candidate, Definitions)) continue;
            const FVector GroundNormal = CourseNormalAt(Candidate, Paths, Definitions, Phase);
            const float SlopeDegrees = FMath::RadiansToDegrees(
                FMath::Acos(FMath::Clamp(GroundNormal.Z, -1.0f, 1.0f)));
            if (SlopeDegrees > 32.0f) continue;
            MaxGroundCoverSlopeDegrees = FMath::Max(MaxGroundCoverSlopeDegrees, SlopeDegrees);

            const float SpeciesRoll = Random.FRand();
            const int32 Species = SpeciesRoll < 0.52f ? 0 : SpeciesRoll < 0.84f ? 1 : 2;
            const float Height = Species == 0 ? Random.FRandRange(12.0f, 25.0f)
                : Species == 1 ? Random.FRandRange(19.0f, 36.0f)
                : Random.FRandRange(25.0f, 44.0f);
            const float Width = Species == 0 ? Random.FRandRange(3.0f, 7.0f)
                : Species == 1 ? Random.FRandRange(5.0f, 10.0f)
                : Random.FRandRange(9.0f, 17.0f);
            const float Yaw = Random.FRandRange(-PI, PI);
            const FVector Base(Candidate.X, Candidate.Y,
                CourseHeightAt(Candidate, Paths, Definitions, Phase) + 1.4f);
            const float Lean = Random.FRandRange(-Height * 0.08f, Height * 0.18f);
            AddVerticalCardInstance(GrassComponents[Species], Base, GroundNormal,
                Yaw, Width, Height, Lean);
            AddVerticalCardInstance(GrassComponents[Species], Base, GroundNormal,
                Yaw + 0.5f * PI, Width * 0.88f,
                Height * Random.FRandRange(0.86f, 1.04f), -Lean * 0.65f);
            ++GrassBladeClusterCount;
            GrassCardInstanceCount += 2;
            if (Path.HoleNumber > 0)
            {
                ++GrassClustersByHole.FindOrAdd(Path.HoleNumber);
                if (Species == 2) ++FernClustersByHole.FindOrAdd(Path.HoleNumber);
            }
        }
        GrassTriangleCount = GrassCardInstanceCount * 2;
        for (const UHierarchicalInstancedStaticMeshComponent* Component : GrassComponents)
        {
            if (Component->GetInstanceCount() > 0) ++GrassSpeciesVariantCount;
        }
    }

    const int32 LitterTarget = FMath::Clamp(
        FMath::RoundToInt(1800.0f * FMath::Clamp(
            RuntimeGroundCoverDensityScale, 0.0f, 1.5f)),
        0, 2700);
    if (LitterTarget > 0 && bGroundCoverAssetsReady)
    {
        FRandomStream LitterRandom(CombinedSeed ^ 0x2c1b3c6d);
        const int32 MaxAttempts = LitterTarget * 14;
        for (int32 Attempt = 0;
            Attempt < MaxAttempts && LitterClusterCount < LitterTarget;
            ++Attempt)
        {
            const FTerrainPath& Path = Paths[LitterRandom.RandRange(0, Paths.Num() - 1)];
            FVector Center;
            FVector2D Direction;
            if (!SamplePolyline(Path.Points, LitterRandom.FRand(), Center, Direction)) continue;
            const FVector2D Side(-Direction.Y, Direction.X);
            const float SideSign = LitterRandom.RandRange(0, 1) == 0 ? -1.0f : 1.0f;
            const float Lateral = SideSign * LitterRandom.FRandRange(
                Path.HalfWidthCm + 520.0f, Path.bConnector ? 2500.0f : 6200.0f);
            const FVector2D Candidate = FVector2D(Center) + Side * Lateral
                + Direction * LitterRandom.FRandRange(-260.0f, 260.0f);
            if (IsProtectedGrassCandidate(Candidate, Definitions)) continue;
            const FVector GroundNormal = CourseNormalAt(Candidate, Paths, Definitions, Phase);
            const float SlopeDegrees = FMath::RadiansToDegrees(
                FMath::Acos(FMath::Clamp(GroundNormal.Z, -1.0f, 1.0f)));
            if (SlopeDegrees > 38.0f) continue;
            MaxGroundCoverSlopeDegrees = FMath::Max(MaxGroundCoverSlopeDegrees, SlopeDegrees);

            const FVector LitterCenter(Candidate.X, Candidate.Y,
                CourseHeightAt(Candidate, Paths, Definitions, Phase) + 1.0f);
            const float HeightAlpha = FMath::Clamp((LitterCenter.Z + 300.0f) / 1800.0f,
                0.0f, 1.0f);
            const float NeedleChance = FMath::Clamp(0.42f
                + 0.20f * (SlopeDegrees / 38.0f) + 0.16f * HeightAlpha,
                0.38f, 0.76f);
            // Higher/steeper forest floor gathers more needles; broad leaf litter remains
            // dominant in low, flatter pockets. Total presentation population is unchanged.
            const int32 Variant = LitterRandom.FRand() < NeedleChance ? 1 : 0;
            AddGroundCardInstance(LitterComponents[Variant], LitterCenter, GroundNormal,
                LitterRandom.FRandRange(-PI, PI),
                Variant == 0 ? LitterRandom.FRandRange(15.0f, 32.0f)
                    : LitterRandom.FRandRange(7.0f, 15.0f),
                Variant == 0 ? LitterRandom.FRandRange(24.0f, 48.0f)
                    : LitterRandom.FRandRange(30.0f, 60.0f));
            ++LitterClusterCount;
            if (Path.HoleNumber > 0) ++LitterClustersByHole.FindOrAdd(Path.HoleNumber);
        }
        LitterTriangleCount = LitterClusterCount * 2;
        for (const UHierarchicalInstancedStaticMeshComponent* Component : LitterComponents)
        {
            if (Component->GetInstanceCount() > 0) ++LitterVariantCount;
        }
    }

    if (const FProcMeshSection* BaseSection = TerrainMesh->GetProcMeshSection(0))
    {
        FProcMeshSection CollisionBaseSection = *BaseSection;
        CollisionBaseSection.bEnableCollision = true;
        TerrainMesh->SetProcMeshSection(0, CollisionBaseSection);
    }

    CourseHoleCount = Definitions.Num();
    bReady = TriangleCount > 0
        && (Definitions.Num() < 2 || ConnectorTriangleCount == (Definitions.Num() - 1) * 256)
        && (Definitions.Num() < 2 || FairwayTriangleCount == Definitions.Num() * 384)
        && (!LakeSurface || ShorelineTriangleCount == 576)
        && (Definitions.Num() < 2 || TrailWearPatchCount == (Definitions.Num() - 1) * PatchesPerConnector)
        && (Definitions.Num() < 2 || TrailWearTriangleCount == TrailWearPatchCount * 8)
        && (!LakeSurface || bShoreAssetsReady)
        && (!LakeSurface || ShoreRockInstanceCount == 42)
        && (!LakeSurface || ShoreReedClusterCount == 96)
        && (!LakeSurface || ShoreReedStemInstanceCount == ShoreReedClusterCount * 4)
        && (!LakeSurface || ShoreDeadfallInstanceCount == 14)
        && (!LakeSurface || ShoreAccentVariantCount == 3)
        && (!LakeSurface || MinShorelineAccentRouteClearanceCm >= 1050.0f)
        && (GrassTarget == 0 || GrassBladeClusterCount >= FMath::CeilToInt(GrassTarget * 0.95f))
        && (GrassTarget == 0 || GrassCardInstanceCount == GrassBladeClusterCount * 2)
        && (GrassTarget == 0 || GrassSpeciesVariantCount == 3)
        && (LitterTarget == 0 || LitterClusterCount >= FMath::CeilToInt(LitterTarget * 0.95f))
        && (LitterTarget == 0 || LitterVariantCount == 2)
        && (GrassTarget == 0 && LitterTarget == 0 || bGroundCoverAssetsReady)
        && GroundCoverCullStartDistanceCm > 0
        && GroundCoverCullEndDistanceCm > GroundCoverCullStartDistanceCm
        && MaxGroundCoverSlopeDegrees <= 38.0f
        && GetGroundMacroVariationSpan() >= 96
        && (!LakeSurface || MaxShorelineErosionOffsetCm >= 100.0f)
        && IsCollisionInvariant();
    return bReady;
}

bool ADiscGolfTerrainPresentationActor::IsCollisionInvariant() const
{
    if (!TerrainMesh
        || !ActorHasTag(TEXT("Presentation.CourseTerrain"))
        || !TerrainMesh->bUseComplexAsSimpleCollision
        || TerrainMesh->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics
        || TerrainMesh->GetCollisionObjectType() != ECC_WorldStatic
        || TerrainMesh->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block
        || TerrainMesh->GetCollisionResponseToChannel(ECC_WorldDynamic) != ECR_Block
        || TerrainMesh->GetGenerateOverlapEvents()
        || TerrainMesh->CanEverAffectNavigation())
    {
        return false;
    }

    if (bReady)
    {
        UProceduralMeshComponent* MutableTerrain =
            const_cast<UProceduralMeshComponent*>(TerrainMesh.Get());
        const FProcMeshSection* BaseSection = MutableTerrain->GetProcMeshSection(0);
        if (!BaseSection || !BaseSection->bEnableCollision
            || !TerrainMesh->ContainsPhysicsTriMeshData(true))
        {
            return false;
        }
        for (int32 SectionIndex = 1; SectionIndex <= 4; ++SectionIndex)
        {
            const FProcMeshSection* DetailSection =
                MutableTerrain->GetProcMeshSection(SectionIndex);
            if (DetailSection && DetailSection->bEnableCollision) return false;
        }
    }

    for (const UPrimitiveComponent* Component : {
        static_cast<const UPrimitiveComponent*>(GrassMesh.Get()),
        static_cast<const UPrimitiveComponent*>(FineGrassInstances.Get()),
        static_cast<const UPrimitiveComponent*>(SedgeGrassInstances.Get()),
        static_cast<const UPrimitiveComponent*>(FernGrassInstances.Get()),
        static_cast<const UPrimitiveComponent*>(LeafLitterInstances.Get()),
        static_cast<const UPrimitiveComponent*>(NeedleLitterInstances.Get()),
        static_cast<const UPrimitiveComponent*>(ShoreRockInstances.Get()),
        static_cast<const UPrimitiveComponent*>(ShoreReedInstances.Get()),
        static_cast<const UPrimitiveComponent*>(ShoreDeadfallInstances.Get()) })
    {
        if (!Component || Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision
            || Component->GetGenerateOverlapEvents() || Component->CanEverAffectNavigation())
        {
            return false;
        }
    }
    return true;
}

bool ADiscGolfTerrainPresentationActor::IsBaseTerrainCollisionFace(int32 FaceIndex) const
{
    if (!TerrainMesh || FaceIndex < 0) return false;
    int32 SectionIndex = INDEX_NONE;
    TerrainMesh->GetMaterialFromCollisionFaceIndex(FaceIndex, SectionIndex);
    return SectionIndex == 0;
}
