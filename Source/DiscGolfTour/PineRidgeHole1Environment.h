#pragma once

#include "CoreMinimal.h"
#include "DiscGolfEnvironmentTypes.h"
#include "PineRidgeHole1Environment.generated.h"

struct FDiscGolfHoleBlockoutDefinition;

/** Serializable design intent used to build the benchmark hole in the persistent course world. */
USTRUCT()
struct FDiscGolfHole1EnvironmentZonePlan
{
    GENERATED_BODY()

    UPROPERTY() FName ZoneId = NAME_None;
    UPROPERTY() EDiscGolfEnvironmentZoneType ZoneType = EDiscGolfEnvironmentZoneType::Fairway;
    UPROPERTY() EDiscGolfEnvironmentZoneShape Shape = EDiscGolfEnvironmentZoneShape::SplineCorridor;
    UPROPERTY() int32 Priority = 0;
    UPROPERTY() bool bHardExclusion = false;
    UPROPERTY() float WidthCm = 1200.0f;
    UPROPERTY() float RadiusCm = 1000.0f;
    UPROPERTY() FVector BoxExtentCm = FVector(3000.0f, 3000.0f, 1500.0f);
    UPROPERTY() float TreeSetbackCm = 550.0f;
    UPROPERTY() float BrushSetbackCm = 250.0f;
    UPROPERTY() float BlendFalloffCm = 0.0f;
    UPROPERTY() FVector LocationCm = FVector::ZeroVector;
    UPROPERTY() TArray<FVector> SplinePointsCm;
};

USTRUCT()
struct FDiscGolfHole1EnvironmentStatistics
{
    GENERATED_BODY()

    UPROPERTY() int32 TreeInstances = 0;
    UPROPERTY() int32 Saplings = 0;
    UPROPERTY() int32 Shrubs = 0;
    UPROPERTY() int32 Ferns = 0;
    UPROPERTY() int32 GrassGroundCoverInstances = 0;
    UPROPERTY() int32 LogsStumps = 0;
    UPROPERTY() int32 Rocks = 0;
    UPROPERTY() int32 CollisionProxies = 0;
    UPROPERTY() int32 InteractionVolumes = 0;
};

namespace PineRidgeHole1Environment
{
    DISCGOLFTOUR_API TArray<FDiscGolfHole1EnvironmentZonePlan> BuildZonePlan(
        const FDiscGolfHoleBlockoutDefinition& Definition);

    DISCGOLFTOUR_API bool ValidateClearance(
        const FDiscGolfHoleBlockoutDefinition& Definition,
        const TArray<FDiscGolfHole1EnvironmentZonePlan>& Zones,
        FString& OutError);

    DISCGOLFTOUR_API bool ValidateRepresentativeFlightRoutes(
        const FDiscGolfHoleBlockoutDefinition& Definition,
        FString& OutError);
}
