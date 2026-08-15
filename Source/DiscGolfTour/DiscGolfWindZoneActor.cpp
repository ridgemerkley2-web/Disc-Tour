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
