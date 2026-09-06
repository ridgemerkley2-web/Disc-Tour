#include "DiscGolfWindZoneActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ADiscGolfWindZoneActor::ADiscGolfWindZoneActor()
{
    PrimaryActorTick.bCanEverTick = false;
    ZoneBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("WindZoneBounds"));
    SetRootComponent(ZoneBounds);
    ZoneBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visualization = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WindZoneVisualization"));
    Visualization->SetupAttachment(ZoneBounds);
    Visualization->SetMobility(EComponentMobility::Movable);
    Visualization->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visualization->SetCastShadow(false);
    Visualization->SetVisibility(false);
    Tags.AddUnique(TEXT("CourseFeature.WindZone"));
}

void ADiscGolfWindZoneActor::Configure(const FDiscGolfWindZoneDefinition& D, UStaticMesh* VisualizationMesh)
{
    ZoneId = D.ZoneId; BaseWindScale = D.BaseWindScale; AdditiveWindMps = D.AdditiveWindMps;
    SetActorLocation(D.LocationCm); ZoneBounds->SetBoxExtent(D.ExtentCm);
    Visualization->SetStaticMesh(VisualizationMesh);
    Visualization->SetRelativeScale3D(D.ExtentCm / 50.0f);
}

bool ADiscGolfWindZoneActor::ContainsPoint(const FVector& WorldLocationCm) const
{
    return ZoneBounds && ZoneBounds->Bounds.GetBox().IsInside(WorldLocationCm);
}

FVector ADiscGolfWindZoneActor::ModifyWind(const FVector& WindMps) const
{
    return WindMps * BaseWindScale + AdditiveWindMps;
}

bool ADiscGolfWindZoneActor::TryGetPhysicsWorldBounds(
    FBox& OutBounds,
    FString& OutError) const
{
    OutBounds = FBox(ForceInit);
    if (!ZoneBounds)
    {
        OutError = TEXT("Wind zone has no physics bounds component");
        return false;
    }

    const FBox CandidateBounds = ZoneBounds->Bounds.GetBox();
    const bool bFinite = FMath::IsFinite(CandidateBounds.Min.X)
        && FMath::IsFinite(CandidateBounds.Min.Y)
        && FMath::IsFinite(CandidateBounds.Min.Z)
        && FMath::IsFinite(CandidateBounds.Max.X)
        && FMath::IsFinite(CandidateBounds.Max.Y)
        && FMath::IsFinite(CandidateBounds.Max.Z);
    if (!CandidateBounds.IsValid || !bFinite)
    {
        OutError = FString::Printf(
            TEXT("Wind zone '%s' has invalid physics world bounds"),
            *ZoneId.ToString());
        return false;
    }

    const FVector Extent = CandidateBounds.GetExtent();
    const bool bFiniteExtent = FMath::IsFinite(Extent.X)
        && FMath::IsFinite(Extent.Y)
        && FMath::IsFinite(Extent.Z);
    if (!bFiniteExtent || Extent.GetMin() <= SMALL_NUMBER)
    {
        OutError = FString::Printf(
            TEXT("Wind zone '%s' has invalid physics world bounds"),
            *ZoneId.ToString());
        return false;
    }

    OutBounds = CandidateBounds;
    OutError.Reset();
    return true;
}
