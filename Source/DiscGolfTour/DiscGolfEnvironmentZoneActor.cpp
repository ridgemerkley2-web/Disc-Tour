#include "DiscGolfEnvironmentZoneActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

namespace
{
    bool UsesTreeSetback(EDiscGolfEnvironmentAssetCategory Category)
    {
        return Category == EDiscGolfEnvironmentAssetCategory::TreeConiferLarge
            || Category == EDiscGolfEnvironmentAssetCategory::TreeConiferMedium
            || Category == EDiscGolfEnvironmentAssetCategory::TreeConiferYoung
            || Category == EDiscGolfEnvironmentAssetCategory::TreeDeciduousLarge
            || Category == EDiscGolfEnvironmentAssetCategory::TreeDeciduousMedium;
    }

    bool UsesBrushSetback(EDiscGolfEnvironmentAssetCategory Category)
    {
        return Category == EDiscGolfEnvironmentAssetCategory::Sapling
            || Category == EDiscGolfEnvironmentAssetCategory::Shrub
            || Category == EDiscGolfEnvironmentAssetCategory::Fern;
    }
}

ADiscGolfEnvironmentZoneActor::ADiscGolfEnvironmentZoneActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("ZoneSpline"));
    Spline->SetupAttachment(Root);
    Spline->SetClosedLoop(false);

    BoxPreview = CreateDefaultSubobject<UBoxComponent>(TEXT("ZonePreview"));
    BoxPreview->SetupAttachment(Root);
    BoxPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoxPreview->SetGenerateOverlapEvents(false);
    BoxPreview->SetCanEverAffectNavigation(false);
    BoxPreview->SetHiddenInGame(true);

    Tags.AddUnique(TEXT("Environment.Zone"));
    Tags.AddUnique(TEXT("Environment.Source.Zone"));
}

void ADiscGolfEnvironmentZoneActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BoxPreview->SetBoxExtent(BoxExtentCm.GetAbs());
    BoxPreview->SetVisibility(Shape == EDiscGolfEnvironmentZoneShape::Box);
    Spline->SetVisibility(Shape == EDiscGolfEnvironmentZoneShape::SplineCorridor);
    if (bHardExclusion) Tags.AddUnique(TEXT("Environment.Exclusion"));
    else Tags.Remove(TEXT("Environment.Exclusion"));
}

float ADiscGolfEnvironmentZoneActor::GetDistanceToSpline2D(const FVector& WorldLocation) const
{
    if (!Spline || Spline->GetNumberOfSplinePoints() < 2) return TNumericLimits<float>::Max();
    const FVector Closest = Spline->FindLocationClosestToWorldLocation(
        WorldLocation, ESplineCoordinateSpace::World);
    return FVector2D::Distance(FVector2D(Closest), FVector2D(WorldLocation));
}

bool ADiscGolfEnvironmentZoneActor::ContainsForCategory(
    const FVector& WorldLocation,
    EDiscGolfEnvironmentAssetCategory Category) const
{
    switch (Shape)
    {
        case EDiscGolfEnvironmentZoneShape::SplineCorridor:
        {
            const float Setback = UsesTreeSetback(Category) ? TreeSetbackCm
                : UsesBrushSetback(Category) ? BrushSetbackCm : 0.0f;
            return GetDistanceToSpline2D(WorldLocation) <= WidthCm * 0.5f + Setback;
        }
        case EDiscGolfEnvironmentZoneShape::Radial:
            return FVector2D::Distance(FVector2D(GetActorLocation()), FVector2D(WorldLocation))
                <= RadiusCm;
        case EDiscGolfEnvironmentZoneShape::Box:
        default:
        {
            const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
            const FVector Extent = BoxExtentCm.GetAbs();
            return FMath::Abs(Local.X) <= Extent.X
                && FMath::Abs(Local.Y) <= Extent.Y
                && FMath::Abs(Local.Z) <= Extent.Z;
        }
    }
}

float ADiscGolfEnvironmentZoneActor::GetInfluenceForCategory(
    const FVector& WorldLocation,
    EDiscGolfEnvironmentAssetCategory Category) const
{
    const float SafeFalloff = FMath::Max(0.0f, BlendFalloffCm);
    switch (Shape)
    {
        case EDiscGolfEnvironmentZoneShape::SplineCorridor:
        {
            const float Setback = UsesTreeSetback(Category) ? TreeSetbackCm
                : UsesBrushSetback(Category) ? BrushSetbackCm : 0.0f;
            const float CoreRadius = WidthCm * 0.5f + Setback;
            const float Distance = GetDistanceToSpline2D(WorldLocation);
            if (Distance <= CoreRadius) return 1.0f;
            return SafeFalloff > 0.0f
                ? 1.0f - FMath::Clamp((Distance - CoreRadius) / SafeFalloff, 0.0f, 1.0f)
                : 0.0f;
        }
        case EDiscGolfEnvironmentZoneShape::Radial:
        {
            const float Distance = FVector2D::Distance(
                FVector2D(GetActorLocation()), FVector2D(WorldLocation));
            if (Distance <= RadiusCm) return 1.0f;
            return SafeFalloff > 0.0f
                ? 1.0f - FMath::Clamp((Distance - RadiusCm) / SafeFalloff, 0.0f, 1.0f)
                : 0.0f;
        }
        case EDiscGolfEnvironmentZoneShape::Box:
        default:
        {
            const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation).GetAbs();
            const FVector Extent = BoxExtentCm.GetAbs();
            if (Local.X <= Extent.X && Local.Y <= Extent.Y && Local.Z <= Extent.Z) return 1.0f;
            if (SafeFalloff <= 0.0f) return 0.0f;
            const FVector Outside = (Local - Extent).ComponentMax(FVector::ZeroVector);
            return 1.0f - FMath::Clamp(Outside.Size() / SafeFalloff, 0.0f, 1.0f);
        }
    }
}
