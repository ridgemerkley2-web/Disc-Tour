#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseDefinition.h"

struct FDiscGolfRouteTelemetryEvaluation
{
    bool bLandingZoneHit = false;
    FString MissSide = TEXT("Unclassified");
    int32 CorridorSampleCount = 0;
    int32 CorridorSamplesInside = 0;
    float CorridorAdherencePercent = 0.0f;
    float MaximumCorridorDeviationMeters = 0.0f;
};

struct FDiscGolfRouteTelemetryAttempt
{
    int32 AttemptNumber = 0;
    FName RouteId = NAME_None;
    FString RecordedUtc;
    FVector ReleaseLocationCm = FVector::ZeroVector;
    FVector RawFinalLocationCm = FVector::ZeroVector;
    FVector LieLocationCm = FVector::ZeroVector;
    bool bLandingZoneHit = false;
    FString MissSide = TEXT("Unclassified");
    int32 CorridorSampleCount = 0;
    int32 CorridorSamplesInside = 0;
    float CorridorAdherencePercent = 0.0f;
    float MaximumCorridorDeviationMeters = 0.0f;
    EDiscGolfPenaltyType PenaltyType = EDiscGolfPenaltyType::None;
    int32 PenaltyStrokes = 0;
    float RemainingDistanceMeters = 0.0f;
    bool bBasketVisible = false;
    int32 TradeoffUnderstood = -1;
    int32 NextShotClear = -1;
    int32 FinalHoleScore = -1;
    bool bHoledOutOnRouteShot = false;
    int32 FixtureContactCount = 0;
    EDiscGolfFixtureType LastFixtureType = EDiscGolfFixtureType::Unknown;
    float FlightTimeSeconds = 0.0f;
    float FinalCarryMeters = 0.0f;
    FName MoldId = NAME_None;
    EDiscPlastic Plastic = EDiscPlastic::Base;
    EThrowStyle ThrowStyle = EThrowStyle::Backhand;
    float ReleaseSpeedMps = 0.0f;
    float ReleaseSpinRpm = 0.0f;
    float ReleaseHyzerDeg = 0.0f;
    float ReleaseNoseDeg = 0.0f;
    float ReleaseLaunchDeg = 0.0f;
    float ReleaseQuality01 = 0.0f;
};

struct FDiscGolfRouteTelemetrySession
{
    int32 SchemaVersion = 1;
    FString SessionId;
    FString StartedUtc;
    FString UpdatedUtc;
    FName CourseId = NAME_None;
    FName LayoutId = NAME_None;
    FName CollisionProfileId = NAME_None;
    int32 HoleNumber = 2;
    int32 TargetAttemptsPerRoute = 20;
    FName ActiveRouteId = NAME_None;
    TArray<FDiscGolfShotRouteDefinition> Routes;
    TArray<FDiscGolfLandingZoneDefinition> LandingZones;
    TArray<FDiscGolfRouteTelemetryAttempt> Attempts;
};

namespace DiscGolfRouteTelemetry
{
    DISCGOLFTOUR_API FString RouteTypeName(EDiscGolfShotRouteType Type);
    DISCGOLFTOUR_API FString FixtureTypeName(EDiscGolfFixtureType Type);
    DISCGOLFTOUR_API FDiscGolfRouteTelemetryEvaluation EvaluateRouteShot(
        const FDiscGolfShotRouteDefinition& Route,
        const FDiscGolfLandingZoneDefinition& LandingZone,
        const TArray<FDiscTrajectorySample>& Samples,
        const FVector& RawFinalLocationCm);
    DISCGOLFTOUR_API int32 CountAttempts(
        const FDiscGolfRouteTelemetrySession& Session,
        FName RouteId);
    DISCGOLFTOUR_API bool IsComplete(const FDiscGolfRouteTelemetrySession& Session);
    DISCGOLFTOUR_API bool IsCompatible(
        const FDiscGolfRouteTelemetrySession& Session,
        FName CourseId,
        FName LayoutId,
        FName CollisionProfileId,
        int32 HoleNumber);
    DISCGOLFTOUR_API bool SerializeSession(
        const FDiscGolfRouteTelemetrySession& Session,
        FString& OutJson,
        FString& OutError);
    DISCGOLFTOUR_API bool DeserializeSession(
        const FString& Json,
        FDiscGolfRouteTelemetrySession& OutSession,
        FString& OutError);
    DISCGOLFTOUR_API bool SaveSession(
        FDiscGolfRouteTelemetrySession& Session,
        const FString& Directory,
        FString& OutPath,
        FString& OutError);
    DISCGOLFTOUR_API bool LoadSession(
        const FString& Path,
        FDiscGolfRouteTelemetrySession& OutSession,
        FString& OutError);
}
