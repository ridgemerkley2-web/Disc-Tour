#include "DiscGolfFoliagePresentationActor.h"

#include "DiscGolfCourseDefinition.h"
#include "DiscGolfCoursePresentationDefinition.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
    constexpr float BasicShapeHalfExtentCm = 50.0f;
    constexpr float CameraClearingRadiusCm = 1100.0f;
    constexpr float GalleryBufferCm = 350.0f;
    constexpr float LakeForestBufferCm = 700.0f;

    void ConfigureVisualComponent(
        UHierarchicalInstancedStaticMeshComponent* Component,
        UStaticMesh* Mesh,
        float CullDistanceScale)
    {
        if (!Component) return;
        Component->SetStaticMesh(Mesh);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        Component->SetCastShadow(true);
        Component->SetCullDistances(
            FMath::RoundToInt(7000.0f * CullDistanceScale),
            FMath::RoundToInt(20800.0f * CullDistanceScale));
    }

    FVector PivotCorrectedLocation(
        const UStaticMesh* Mesh,
        const FVector& DesiredBase,
        const FRotator& Rotation,
        float Scale)
    {
        if (!Mesh) return DesiredBase;
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FVector LocalBase(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
        return DesiredBase - Rotation.RotateVector(LocalBase * Scale);
    }

    float DistanceToSegment2D(
        const FVector2D& Point,
        const FVector2D& Start,
        const FVector2D& End)
    {
        const FVector2D Segment = End - Start;
        const float LengthSquared = Segment.SizeSquared();
        if (LengthSquared <= KINDA_SMALL_NUMBER) return FVector2D::Distance(Point, Start);
        const float Alpha = FMath::Clamp(FVector2D::DotProduct(Point - Start, Segment) / LengthSquared,
            0.0f, 1.0f);
        return FVector2D::Distance(Point, Start + Segment * Alpha);
    }

    float DistanceToRoute2D(const FVector2D& Point, const TArray<FVector>& Waypoints)
    {
        float Best = TNumericLimits<float>::Max();
        for (int32 Index = 1; Index < Waypoints.Num(); ++Index)
        {
            Best = FMath::Min(Best, DistanceToSegment2D(
                Point, FVector2D(Waypoints[Index - 1]), FVector2D(Waypoints[Index])));
        }
        return Best;
    }

    bool SampleRoute(
        const TArray<FVector>& Waypoints,
        float NormalizedDistance,
        FVector& OutPoint,
        FVector2D& OutTangent)
    {
        float TotalLength = 0.0f;
        for (int32 Index = 1; Index < Waypoints.Num(); ++Index)
        {
            TotalLength += FVector2D::Distance(
                FVector2D(Waypoints[Index - 1]), FVector2D(Waypoints[Index]));
        }
        if (Waypoints.Num() < 2 || TotalLength <= KINDA_SMALL_NUMBER) return false;

        const float Target = FMath::Clamp(NormalizedDistance, 0.0f, 1.0f) * TotalLength;
        float Traversed = 0.0f;
        for (int32 Index = 1; Index < Waypoints.Num(); ++Index)
        {
            const FVector Start = Waypoints[Index - 1];
            const FVector End = Waypoints[Index];
            const float SegmentLength = FVector2D::Distance(FVector2D(Start), FVector2D(End));
            if (SegmentLength <= KINDA_SMALL_NUMBER) continue;
            if (Traversed + SegmentLength >= Target || Index == Waypoints.Num() - 1)
            {
                const float Alpha = FMath::Clamp((Target - Traversed) / SegmentLength, 0.0f, 1.0f);
                OutPoint = FMath::Lerp(Start, End, Alpha);
                OutTangent = (FVector2D(End) - FVector2D(Start)).GetSafeNormal();
                return !OutTangent.IsNearlyZero();
            }
            Traversed += SegmentLength;
        }
        return false;
    }

    bool IsInsideWaterExclusion(
        const FVector2D& Candidate,
        const FDiscGolfBlockoutSurfaceDefinition& Surface)
    {
        if (!Surface.SurfaceId.ToString().Contains(TEXT("LakeWater"))) return false;
        const FVector Local = Surface.Rotation.UnrotateVector(
            FVector(Candidate.X, Candidate.Y, Surface.LocationCm.Z) - Surface.LocationCm);
        const float RadiusX = BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.X) + LakeForestBufferCm;
        const float RadiusY = BasicShapeHalfExtentCm * FMath::Abs(Surface.Scale.Y) + LakeForestBufferCm;
        if (Surface.Shape == EDiscGolfPrimitiveShape::Cylinder)
        {
            return FMath::Square(Local.X / RadiusX) + FMath::Square(Local.Y / RadiusY) <= 1.0f;
        }
        return FMath::Abs(Local.X) <= RadiusX && FMath::Abs(Local.Y) <= RadiusY;
    }

    bool IsInsideGalleryExclusion(
        const FVector2D& Candidate,
        const FDiscGolfSpectatorBoundaryDefinition& Boundary)
    {
        const FVector Local = Boundary.Rotation.UnrotateVector(
            FVector(Candidate.X, Candidate.Y, Boundary.LocationCm.Z) - Boundary.LocationCm);
        return FMath::Abs(Local.X) <= Boundary.ExtentCm.X + GalleryBufferCm
            && FMath::Abs(Local.Y) <= Boundary.ExtentCm.Y + GalleryBufferCm;
    }

    bool IsProtectedCandidate(
        const FVector2D& Candidate,
        const FDiscGolfHoleBlockoutDefinition& Hole,
        const FDiscGolfHoleVisualPlan& Plan)
    {
        if (FVector2D::Distance(Candidate, FVector2D(Hole.TeeLocationCm)) < Plan.TeeClearingRadiusCm
            || FVector2D::Distance(Candidate, FVector2D(Hole.BasketLocationCm)) < Plan.GreenClearingRadiusCm)
        {
            return true;
        }
        for (const FDiscGolfShotRouteDefinition& Route : Hole.ShotRoutes)
        {
            if (DistanceToRoute2D(Candidate, Route.WaypointsCm)
                < Route.CorridorWidthCm * 0.5f + Plan.CorridorBufferCm)
            {
                return true;
            }
        }
        for (const FDiscGolfCameraAnchorDefinition& Camera : Hole.CameraAnchors)
        {
            if (FVector2D::Distance(Candidate, FVector2D(Camera.LocationCm)) < CameraClearingRadiusCm)
            {
                return true;
            }
        }
        for (const FDiscGolfSpectatorBoundaryDefinition& Boundary : Hole.SpectatorBoundaries)
        {
            if (IsInsideGalleryExclusion(Candidate, Boundary)) return true;
        }
        for (const FDiscGolfBlockoutSurfaceDefinition& Surface : Hole.Surfaces)
        {
            if (IsInsideWaterExclusion(Candidate, Surface)) return true;
        }
        return false;
    }
}

int32 DiscGolfFoliagePresentation::ResolveDecorativeInstanceTarget(
    const FDiscGolfHoleVisualPlan& Plan,
    float QualityDensityScale)
{
    const float SafeCanopy = FMath::Clamp(Plan.CanopyDensityScale, 0.0f, 1.5f);
    const float SafeQuality = FMath::Clamp(QualityDensityScale, 0.0f, 1.5f);
    return FMath::Clamp(FMath::RoundToInt(360.0f * SafeCanopy * SafeQuality), 0, 600);
}

ADiscGolfFoliagePresentationActor::ADiscGolfFoliagePresentationActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    SaplingA = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SaplingA"));
    SaplingA->SetupAttachment(SceneRoot);
    SaplingB = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SaplingB"));
    SaplingB->SetupAttachment(SceneRoot);
    SaplingC = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SaplingC"));
    SaplingC->SetupAttachment(SceneRoot);
    ShadowSaplingA = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ShadowSaplingA"));
    ShadowSaplingA->SetupAttachment(SceneRoot);
    ShadowSaplingB = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ShadowSaplingB"));
    ShadowSaplingB->SetupAttachment(SceneRoot);
    ShadowSaplingC = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ShadowSaplingC"));
    ShadowSaplingC->SetupAttachment(SceneRoot);

    ConfigureVisualComponent(SaplingA, nullptr, 1.0f);
    ConfigureVisualComponent(SaplingB, nullptr, 1.0f);
    ConfigureVisualComponent(SaplingC, nullptr, 1.0f);
    ConfigureVisualComponent(ShadowSaplingA, nullptr, 1.0f);
    ConfigureVisualComponent(ShadowSaplingB, nullptr, 1.0f);
    ConfigureVisualComponent(ShadowSaplingC, nullptr, 1.0f);
}

bool ADiscGolfFoliagePresentationActor::Configure(
    const FDiscGolfHoleBlockoutDefinition& HoleDefinition,
    const FDiscGolfHoleVisualPlan& Plan,
    float QualityDensityScale,
    float QualityCullDistanceScale)
{
    bReady = false;
    DecorativeInstanceCount = 0;
    DecorativeInstanceTarget = DiscGolfFoliagePresentation::ResolveDecorativeInstanceTarget(
        Plan, QualityDensityScale);
    ForestReferenceId = Plan.ForestReferenceId;

    UStaticMesh* Meshes[] =
    {
        LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_a.fir_sapling_a")),
        LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_b.fir_sapling_b")),
        LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_c.fir_sapling_c"))
    };
    UHierarchicalInstancedStaticMeshComponent* DeepComponents[] = { SaplingA, SaplingB, SaplingC };
    UHierarchicalInstancedStaticMeshComponent* ShadowComponents[] =
        { ShadowSaplingA, ShadowSaplingB, ShadowSaplingC };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(DeepComponents); ++Index)
    {
        DeepComponents[Index]->ClearInstances();
        ShadowComponents[Index]->ClearInstances();
        ConfigureVisualComponent(DeepComponents[Index], Meshes[Index], QualityCullDistanceScale);
        ConfigureVisualComponent(ShadowComponents[Index], Meshes[Index], QualityCullDistanceScale);
        DeepComponents[Index]->SetCastShadow(false);
        ShadowComponents[Index]->SetCastShadow(true);
    }
    if (!GetWorld() || !Meshes[0] || !Meshes[1] || !Meshes[2]
        || DecorativeInstanceTarget <= 0 || HoleDefinition.ShotRoutes.IsEmpty())
    {
        return false;
    }

    const FDiscGolfShotRouteDefinition* PrimaryRoute = HoleDefinition.ShotRoutes.FindByPredicate(
        [](const FDiscGolfShotRouteDefinition& Route)
        {
            return Route.RouteType == EDiscGolfShotRouteType::Primary;
        });
    if (!PrimaryRoute || PrimaryRoute->WaypointsCm.Num() < 2) return false;

    const float PrimaryHalfWidth = PrimaryRoute->CorridorWidthCm * 0.5f;
    const float ForestEdgeCm = PrimaryHalfWidth + Plan.CorridorBufferCm + 150.0f;
    FRandomStream Random(Plan.FoliageSeed);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PineRidgeVisualFoliage), false, this);
    TArray<FVector2D> AcceptedPositions;
    AcceptedPositions.Reserve(DecorativeInstanceTarget);

    const int32 MaxAttempts = DecorativeInstanceTarget * 80;
    for (int32 Attempt = 0;
        Attempt < MaxAttempts && DecorativeInstanceCount < DecorativeInstanceTarget;
        ++Attempt)
    {
        const float Along = Random.FRandRange(0.01f, 0.995f);
        FVector RoutePoint;
        FVector2D Tangent;
        if (!SampleRoute(PrimaryRoute->WaypointsCm, Along, RoutePoint, Tangent)) continue;
        const FVector2D Side(-Tangent.Y, Tangent.X);
        const float Sign = Random.RandRange(0, 1) == 0 ? -1.0f : 1.0f;
        const float DepthAlpha = FMath::Pow(Random.FRand(), 2.40f);
        const float Lateral = ForestEdgeCm + Plan.ForestDepthCm * DepthAlpha;
        const FVector2D Candidate = FVector2D(RoutePoint) + Side * (Sign * Lateral)
            + Tangent * Random.FRandRange(-420.0f, 420.0f);
        if (IsProtectedCandidate(Candidate, HoleDefinition, Plan)) continue;

        const float MinimumSpacingCm = DepthAlpha < 0.5f ? 220.0f : 180.0f;
        bool bTooClose = false;
        for (const FVector2D& Existing : AcceptedPositions)
        {
            if (FVector2D::DistSquared(Candidate, Existing) < FMath::Square(MinimumSpacingCm))
            {
                bTooClose = true;
                break;
            }
        }
        if (bTooClose) continue;

        FVector DesiredBase(Candidate.X, Candidate.Y, RoutePoint.Z);
        FHitResult GroundHit;
        if (GetWorld()->LineTraceSingleByChannel(
            GroundHit,
            DesiredBase + FVector(0.0f, 0.0f, 1400.0f),
            DesiredBase - FVector(0.0f, 0.0f, 900.0f),
            ECC_Visibility,
            QueryParams))
        {
            DesiredBase = GroundHit.ImpactPoint;
        }

        const int32 Variant = Random.RandRange(0, UE_ARRAY_COUNT(DeepComponents) - 1);
        const bool bUnderstory = Random.FRand() < 0.32f;
        const float MatureScale = DepthAlpha < 0.48f
            ? Random.FRandRange(7.5f, 11.5f)
            : Random.FRandRange(5.8f, 9.8f);
        const float UniformScale = bUnderstory ? MatureScale * Random.FRandRange(0.34f, 0.55f)
            : MatureScale;
        const FRotator Rotation(0.0f, Random.FRandRange(-180.0f, 180.0f), 0.0f);
        const FVector InstanceLocation = PivotCorrectedLocation(
            Meshes[Variant], DesiredBase, Rotation, UniformScale);
        // Preserve high-value moving canopy shadows along the playable route edge;
        // deeper mass remains visually dense without duplicating the shadow pass.
        UHierarchicalInstancedStaticMeshComponent* InstanceComponent = DepthAlpha < 0.035f
            ? ShadowComponents[Variant] : DeepComponents[Variant];
        InstanceComponent->AddInstance(
            FTransform(Rotation, InstanceLocation, FVector(UniformScale)), true);
        AcceptedPositions.Add(Candidate);
        ++DecorativeInstanceCount;
    }

    bReady = DecorativeInstanceCount >= FMath::CeilToInt(DecorativeInstanceTarget * 0.85f);
    if (!bReady)
    {
        UHierarchicalInstancedStaticMeshComponent* AllComponents[] =
        {
            SaplingA, SaplingB, SaplingC,
            ShadowSaplingA, ShadowSaplingB, ShadowSaplingC
        };
        for (UHierarchicalInstancedStaticMeshComponent* Component : AllComponents)
        {
            Component->ClearInstances();
        }
        DecorativeInstanceCount = 0;
    }
    return bReady;
}

void ADiscGolfFoliagePresentationActor::AddAuthoredTreeVisual(
    const FVector& Location,
    float HeightScale,
    int32 VariantIndex)
{
    UHierarchicalInstancedStaticMeshComponent* Components[] =
        { ShadowSaplingA, ShadowSaplingB, ShadowSaplingC };
    if (VariantIndex < 0) VariantIndex = -VariantIndex;
    const int32 Variant = VariantIndex % UE_ARRAY_COUNT(Components);
    UHierarchicalInstancedStaticMeshComponent* Component = Components[Variant];
    UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
    if (!bReady || !Component || !Mesh) return;

    const float UniformScale = 6.3f * FMath::Max(0.1f, HeightScale);
    const FRotator Rotation(0.0f, static_cast<float>((VariantIndex * 137) % 360), 0.0f);
    const FVector InstanceLocation = PivotCorrectedLocation(Mesh, Location, Rotation, UniformScale);
    Component->AddInstance(FTransform(Rotation, InstanceLocation, FVector(UniformScale)), true);
}

int32 ADiscGolfFoliagePresentationActor::GetVisualInstanceCount() const
{
    return (SaplingA ? SaplingA->GetInstanceCount() : 0)
        + (SaplingB ? SaplingB->GetInstanceCount() : 0)
        + (SaplingC ? SaplingC->GetInstanceCount() : 0)
        + (ShadowSaplingA ? ShadowSaplingA->GetInstanceCount() : 0)
        + (ShadowSaplingB ? ShadowSaplingB->GetInstanceCount() : 0)
        + (ShadowSaplingC ? ShadowSaplingC->GetInstanceCount() : 0);
}

bool ADiscGolfFoliagePresentationActor::IsCollisionInvariant() const
{
    const UHierarchicalInstancedStaticMeshComponent* Components[] =
    {
        SaplingA, SaplingB, SaplingC,
        ShadowSaplingA, ShadowSaplingB, ShadowSaplingC
    };
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!Component || Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision
            || Component->GetGenerateOverlapEvents())
        {
            return false;
        }
    }
    return true;
}
