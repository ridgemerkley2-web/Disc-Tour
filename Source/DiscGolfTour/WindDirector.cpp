#include "WindDirector.h"

#include "DiscGolfWindZoneActor.h"
#include "EngineUtils.h"

namespace
{
constexpr float MaximumBaseWindMps = 25.0f;
constexpr float MaximumGustAmplitudeMps = 15.0f;
constexpr float MinimumGustFrequencyHz = 0.01f;
constexpr float MaximumGustFrequencyHz = 2.0f;
constexpr float MaximumZoneAdditiveWindMps = 25.0f;
constexpr float MaximumSampledWindMps = 200.0f;
constexpr float MaximumPhysicsWindTimeSeconds = 86400.0f;
constexpr double DeterministicPhaseCycleSeconds = 4096.0;
constexpr double DeterministicPhaseStrideSeconds = 37.0;

bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

bool ValidateWindValues(
    const FVector& BaseWindMps,
    float GustAmplitudeMps,
    float GustFrequencyHz,
    FString& OutError)
{
    if (!IsFiniteVector(BaseWindMps)
        || !FMath::IsFinite(BaseWindMps.SizeSquared())
        || BaseWindMps.Size() > MaximumBaseWindMps)
    {
        OutError = FString::Printf(
            TEXT("Base wind must be finite and no greater than %.1f m/s"),
            MaximumBaseWindMps);
        return false;
    }
    if (!FMath::IsFinite(GustAmplitudeMps)
        || GustAmplitudeMps < 0.0f
        || GustAmplitudeMps > MaximumGustAmplitudeMps)
    {
        OutError = FString::Printf(
            TEXT("Gust amplitude must be finite and in [0, %.1f] m/s"),
            MaximumGustAmplitudeMps);
        return false;
    }
    if (!FMath::IsFinite(GustFrequencyHz)
        || GustFrequencyHz < MinimumGustFrequencyHz
        || GustFrequencyHz > MaximumGustFrequencyHz)
    {
        OutError = FString::Printf(
            TEXT("Gust frequency must be finite and in [%.2f, %.1f] Hz"),
            MinimumGustFrequencyHz, MaximumGustFrequencyHz);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSampledWind(const FVector& WindMps, FString& OutError)
{
    if (!IsFiniteVector(WindMps)
        || !FMath::IsFinite(WindMps.SizeSquared())
        || WindMps.Size() > MaximumSampledWindMps)
    {
        OutError = FString::Printf(
            TEXT("Sampled wind must be finite and no greater than %.1f m/s"),
            MaximumSampledWindMps);
        return false;
    }
    return true;
}
}

bool FDiscGolfPhysicsWindZoneSnapshot::Equals(
    const FDiscGolfPhysicsWindZoneSnapshot& Other) const
{
    return ZoneActor == Other.ZoneActor
        && ZoneId == Other.ZoneId
        && BaseWindScale == Other.BaseWindScale
        && AdditiveWindMps == Other.AdditiveWindMps
        && WorldBounds == Other.WorldBounds;
}

bool FDiscGolfPhysicsWindConfigurationSnapshot::Equals(
    const FDiscGolfPhysicsWindConfigurationSnapshot& Other) const
{
    if (!bCaptured || !Other.bCaptured
        || BaseWindMps != Other.BaseWindMps
        || GustAmplitudeMps != Other.GustAmplitudeMps
        || GustFrequencyHz != Other.GustFrequencyHz
        || Zones.Num() != Other.Zones.Num())
    {
        return false;
    }

    for (int32 Index = 0; Index < Zones.Num(); ++Index)
    {
        if (!Zones[Index].Equals(Other.Zones[Index]))
        {
            return false;
        }
    }
    return true;
}

AWindDirector::AWindDirector()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AWindDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.0f)
    {
        SimTime = static_cast<float>(FMath::Fmod(
            static_cast<double>(SimTime) + static_cast<double>(DeltaSeconds),
            DeterministicPhaseCycleSeconds));
    }
}

bool AWindDirector::TryConfigurePhysicsWind(
    const FVector& InBaseWindMps,
    float InGustAmplitudeMps,
    float InGustFrequencyHz,
    FString& OutError)
{
    if (!ValidateWindValues(
        InBaseWindMps, InGustAmplitudeMps, InGustFrequencyHz, OutError))
    {
        return false;
    }
    if (!ValidatePhysicsWindZones(OutError))
    {
        return false;
    }

    BaseWindMps = InBaseWindMps;
    GustAmplitudeMps = InGustAmplitudeMps;
    GustFrequencyHz = InGustFrequencyHz;
    return true;
}

bool AWindDirector::ValidatePhysicsWindConfiguration(FString& OutError) const
{
    if (!ValidateWindValues(BaseWindMps, GustAmplitudeMps, GustFrequencyHz, OutError))
    {
        return false;
    }
    return ValidatePhysicsWindZones(OutError);
}

bool AWindDirector::TryCapturePhysicsWindConfiguration(
    FDiscGolfPhysicsWindConfigurationSnapshot& OutSnapshot,
    FString& OutError) const
{
    OutSnapshot = FDiscGolfPhysicsWindConfigurationSnapshot();
    if (!ValidatePhysicsWindConfiguration(OutError))
    {
        return false;
    }

    FDiscGolfPhysicsWindConfigurationSnapshot Candidate;
    Candidate.BaseWindMps = BaseWindMps;
    Candidate.GustAmplitudeMps = GustAmplitudeMps;
    Candidate.GustFrequencyHz = GustFrequencyHz;
    Candidate.Zones.Reserve(CourseZones.Num());
    for (const TWeakObjectPtr<ADiscGolfWindZoneActor>& ZonePtr : CourseZones)
    {
        ADiscGolfWindZoneActor* Zone = ZonePtr.Get();
        if (!IsValid(Zone))
        {
            OutError = TEXT("Physics wind zone membership contains an unavailable actor");
            return false;
        }

        FDiscGolfPhysicsWindZoneSnapshot ZoneSnapshot;
        ZoneSnapshot.ZoneActor = Zone;
        ZoneSnapshot.ZoneId = Zone->ZoneId;
        ZoneSnapshot.BaseWindScale = Zone->BaseWindScale;
        ZoneSnapshot.AdditiveWindMps = Zone->AdditiveWindMps;
        if (!Zone->TryGetPhysicsWorldBounds(ZoneSnapshot.WorldBounds, OutError))
        {
            return false;
        }
        Candidate.Zones.Add(MoveTemp(ZoneSnapshot));
    }
    Candidate.bCaptured = true;

    OutSnapshot = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

bool AWindDirector::MatchesPhysicsWindConfiguration(
    const FDiscGolfPhysicsWindConfigurationSnapshot& ExpectedSnapshot,
    FString& OutError) const
{
    if (!ExpectedSnapshot.bCaptured)
    {
        OutError = TEXT("Accepted physics wind configuration was not captured");
        return false;
    }

    if (!ValidatePhysicsWindConfiguration(OutError))
    {
        return false;
    }
    if (BaseWindMps != ExpectedSnapshot.BaseWindMps
        || GustAmplitudeMps != ExpectedSnapshot.GustAmplitudeMps
        || GustFrequencyHz != ExpectedSnapshot.GustFrequencyHz
        || CourseZones.Num() != ExpectedSnapshot.Zones.Num())
    {
        OutError = TEXT("Physics wind configuration changed after authoritative launch");
        return false;
    }

    for (int32 Index = 0; Index < CourseZones.Num(); ++Index)
    {
        const TWeakObjectPtr<ADiscGolfWindZoneActor>& ZonePtr = CourseZones[Index];
        const ADiscGolfWindZoneActor* Zone = ZonePtr.Get();
        const FDiscGolfPhysicsWindZoneSnapshot& ExpectedZone =
            ExpectedSnapshot.Zones[Index];
        FBox WorldBounds(ForceInit);
        if (!Zone->TryGetPhysicsWorldBounds(WorldBounds, OutError))
        {
            return false;
        }
        if (ZonePtr != ExpectedZone.ZoneActor
            || Zone->ZoneId != ExpectedZone.ZoneId
            || Zone->BaseWindScale != ExpectedZone.BaseWindScale
            || Zone->AdditiveWindMps != ExpectedZone.AdditiveWindMps
            || WorldBounds != ExpectedZone.WorldBounds)
        {
            OutError = TEXT("Physics wind configuration changed after authoritative launch");
            return false;
        }
    }

    OutError.Reset();
    return true;
}

bool AWindDirector::ValidatePhysicsWindZones(FString& OutError) const
{
    TSet<FName> LiveZoneIds;
    for (const TWeakObjectPtr<ADiscGolfWindZoneActor>& ZonePtr : CourseZones)
    {
        const ADiscGolfWindZoneActor* Zone = ZonePtr.Get();
        if (!IsValid(Zone))
        {
            OutError = TEXT("Physics wind zone membership contains an unavailable actor");
            return false;
        }
        if (Zone->ZoneId.IsNone())
        {
            OutError = TEXT("A live wind zone is missing its deterministic ZoneId");
            return false;
        }
        if (LiveZoneIds.Contains(Zone->ZoneId))
        {
            OutError = FString::Printf(
                TEXT("Live wind zone id '%s' is duplicated"),
                *Zone->ZoneId.ToString());
            return false;
        }
        LiveZoneIds.Add(Zone->ZoneId);
        if (!FMath::IsFinite(Zone->BaseWindScale)
            || Zone->BaseWindScale < 0.0f
            || Zone->BaseWindScale > 3.0f
            || !IsFiniteVector(Zone->AdditiveWindMps)
            || !FMath::IsFinite(Zone->AdditiveWindMps.SizeSquared())
            || Zone->AdditiveWindMps.Size() > MaximumZoneAdditiveWindMps)
        {
            OutError = FString::Printf(
                TEXT("Wind zone '%s' has an unsafe scale or additive wind"),
                *Zone->ZoneId.ToString());
            return false;
        }
        FBox PhysicsBounds(ForceInit);
        if (!Zone->TryGetPhysicsWorldBounds(PhysicsBounds, OutError))
        {
            return false;
        }
    }

    OutError.Reset();
    return true;
}

bool AWindDirector::TryBuildDeterministicShotPhaseOrigin(
    int32 AcceptedShotSequence,
    float& OutPhaseOriginSeconds,
    FString& OutError)
{
    OutPhaseOriginSeconds = 0.0f;
    if (AcceptedShotSequence <= 0)
    {
        OutError = TEXT("Accepted-shot sequence must be positive");
        return false;
    }

    const double PhaseSeconds = FMath::Fmod(
        static_cast<double>(AcceptedShotSequence) * DeterministicPhaseStrideSeconds,
        DeterministicPhaseCycleSeconds);
    if (!FMath::IsFinite(PhaseSeconds)
        || PhaseSeconds < 0.0
        || PhaseSeconds > static_cast<double>(MaximumPhysicsWindTimeSeconds))
    {
        OutError = TEXT("Accepted-shot sequence produced an invalid wind phase origin");
        return false;
    }

    OutPhaseOriginSeconds = static_cast<float>(PhaseSeconds);
    OutError.Reset();
    return true;
}

FVector AWindDirector::GetWindMpsAt(const FVector& WorldLocation) const
{
    return GetWindMpsAtSimulationTime(WorldLocation, SimTime);
}

FVector AWindDirector::GetWindMpsAtSimulationTime(
    const FVector& WorldLocation,
    float SimulationTimeSeconds) const
{
    FVector WindMps = FVector::ZeroVector;
    FString Error;
    return TryGetWindMpsAtSimulationTime(
        WorldLocation, SimulationTimeSeconds, WindMps, Error)
        ? WindMps
        : FVector::ZeroVector;
}

bool AWindDirector::TryGetWindMpsAtSimulationTime(
    const FVector& WorldLocation,
    float SimulationTimeSeconds,
    FVector& OutWindMps,
    FString& OutError) const
{
    OutWindMps = FVector::ZeroVector;
    if (!IsFiniteVector(WorldLocation)
        || !FMath::IsFinite(SimulationTimeSeconds)
        || SimulationTimeSeconds < 0.0f
        || SimulationTimeSeconds > MaximumPhysicsWindTimeSeconds)
    {
        OutError = TEXT("Wind sample location/time must be finite and within the physics time envelope");
        return false;
    }
    if (!ValidatePhysicsWindConfiguration(OutError))
    {
        return false;
    }

    const float SpatialPhase = (WorldLocation.X + WorldLocation.Y * 0.37f) * 0.00022f;
    if (!FMath::IsFinite(SpatialPhase))
    {
        OutError = TEXT("Wind sample spatial phase is non-finite");
        return false;
    }
    const float Gust = FMath::Sin(
        (SimulationTimeSeconds * GustFrequencyHz * 2.0f * PI) + SpatialPhase) * GustAmplitudeMps;
    const FVector Side = FVector(-BaseWindMps.Y, BaseWindMps.X, 0.0f).GetSafeNormal();
    const float SideGust = FMath::Sin(
        SimulationTimeSeconds * 0.53f + SpatialPhase * 1.7f) * GustAmplitudeMps * 0.35f;
    FVector Wind = BaseWindMps + BaseWindMps.GetSafeNormal() * Gust + Side * SideGust;
    if (!ValidateSampledWind(Wind, OutError))
    {
        return false;
    }
    for (const TWeakObjectPtr<ADiscGolfWindZoneActor>& ZonePtr : CourseZones)
    {
        const ADiscGolfWindZoneActor* Zone = ZonePtr.Get();
        if (IsValid(Zone) && Zone->ContainsPoint(WorldLocation))
        {
            Wind = Zone->ModifyWind(Wind);
            if (!ValidateSampledWind(Wind, OutError))
            {
                return false;
            }
        }
    }
    OutWindMps = Wind;
    OutError.Reset();
    return true;
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
