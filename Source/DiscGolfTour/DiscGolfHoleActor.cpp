#include "DiscGolfHoleActor.h"
#include "DiscGolfMath.h"

ADiscGolfHoleActor::ADiscGolfHoleActor()
{
    PrimaryActorTick.bCanEverTick = false;
    HoleName = FText::FromString(TEXT("Pine Ridge Opening"));
}

float ADiscGolfHoleActor::GetMeasuredDistanceFeet() const
{
    return FVector::Dist(TeeLocation, BasketLocation) / 30.48f;
}

float ADiscGolfHoleActor::GetEffectiveDistanceFeet() const
{
    return DiscGolfMath::EffectiveHoleDistanceFeet(TeeLocation, BasketLocation);
}

FString ADiscGolfHoleActor::GetCourseSummary() const
{
    return FString::Printf(TEXT("%s / %s / %s | %d surfaces | %d fixtures | %d landing | %d routes | %d cameras | %d wind"),
        *CourseId.ToString(), *LayoutId.ToString(), *DefinitionSource,
        SurfaceCount, WorldFixtureCount, LandingZoneCount, ShotRouteCount, CameraAnchorCount, WindZoneCount);
}
