#include "WindDirector.h"

#include "DiscGolfWindZoneActor.h"
#include "EngineUtils.h"

AWindDirector::AWindDirector()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AWindDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    SimTime += DeltaSeconds;
}

FVector AWindDirector::GetWindMpsAt(const FVector& WorldLocation) const
{
    const float SpatialPhase = (WorldLocation.X + WorldLocation.Y * 0.37f) * 0.00022f;
    const float Gust = FMath::Sin((SimTime * GustFrequencyHz * 2.0f * PI) + SpatialPhase) * GustAmplitudeMps;
    const FVector Side = FVector(-BaseWindMps.Y, BaseWindMps.X, 0.0f).GetSafeNormal();
    const float SideGust = FMath::Sin(SimTime * 0.53f + SpatialPhase * 1.7f) * GustAmplitudeMps * 0.35f;
    FVector Wind = BaseWindMps + BaseWindMps.GetSafeNormal() * Gust + Side * SideGust;
    for (const TWeakObjectPtr<ADiscGolfWindZoneActor>& ZonePtr : CourseZones)
    {
        const ADiscGolfWindZoneActor* Zone = ZonePtr.Get();
        if (IsValid(Zone) && Zone->ContainsPoint(WorldLocation))
        {
            Wind = Zone->ModifyWind(Wind);
        }
    }
    return Wind;
}

void AWindDirector::RefreshCourseZones()
{
    CourseZones.Reset();
    if (!GetWorld()) return;

    for (TActorIterator<ADiscGolfWindZoneActor> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It)) CourseZones.Add(*It);
    }
    CourseZones.Sort([](const TWeakObjectPtr<ADiscGolfWindZoneActor>& A,
        const TWeakObjectPtr<ADiscGolfWindZoneActor>& B)
    {
        const FName AId = A.IsValid() ? A->ZoneId : NAME_None;
        const FName BId = B.IsValid() ? B->ZoneId : NAME_None;
        return AId.LexicalLess(BId);
    });
}

FName AWindDirector::GetActiveZoneIdAt(const FVector& WorldLocation) const
{
    for (const TWeakObjectPtr<ADiscGolfWindZoneActor>& ZonePtr : CourseZones)
    {
        const ADiscGolfWindZoneActor* Zone = ZonePtr.Get();
        if (IsValid(Zone) && Zone->ContainsPoint(WorldLocation)) return Zone->ZoneId;
    }
    return NAME_None;
}
