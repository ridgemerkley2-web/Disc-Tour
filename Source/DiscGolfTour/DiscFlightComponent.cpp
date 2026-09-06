#include "DiscFlightComponent.h"
#include "DiscGolfCourseRules.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTour.h"
#include "WindDirector.h"
#include "DiscGolfMath.h"
#include "DiscGolfWorldFixtureActor.h"
#include "DiscGolfVegetationInteractionActor.h"
#include "Engine/ScopedMovementUpdate.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
constexpr float MinimumAirDensityKgM3 = 0.01f;
constexpr float MaximumAirDensityKgM3 = 5.0f;
constexpr float MinimumFixedStepSeconds = 0.001f;
constexpr float MaximumFixedStepSeconds = 1.0f / 30.0f;
constexpr float MinimumFlightDurationSeconds = 0.1f;
constexpr float MaximumFlightDurationSeconds = 120.0f;
constexpr float MinimumGroundPlayDurationSeconds = 0.1f;
constexpr float MaximumGroundPlayDurationSeconds = 60.0f;
constexpr float MinimumPrecessionRateRadPerSec = 0.01f;
constexpr float MaximumPrecessionRateRadPerSec = 20.0f;
constexpr float MaximumGroundContactBiasMps = 5.0f;
constexpr float MinimumGroundProbeDepthCm = 1.0f;
constexpr float MaximumGroundProbeDepthCm = 1000.0f;
constexpr float MinimumTrajectorySampleHz = 1.0f;
constexpr float MaximumTrajectorySampleHz = 1000.0f;
constexpr float MaximumRuntimeVelocityMps = 250.0f;
constexpr float MaximumRuntimeAccelerationMps2 = 500.0f;
constexpr float MaximumRuntimeDisplacementCm = 2000000.0f;
constexpr float MaximumBasketCaptureSnapCm = 250.0f;
constexpr float MaximumRuntimeWindMps = 200.0f;
constexpr int32 MaximumRuntimeContactCount = 100000;
constexpr int32 MaximumFixedStepsPerTick = 128;
constexpr double MaximumAcceptedTickDeltaSeconds = 0.1;

bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z)
        && FMath::IsFinite(Value.SizeSquared());
}

bool IsFiniteQuaternion(const FQuat& Value)
{
    return FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z)
        && FMath::IsFinite(Value.W)
        && Value.IsNormalized();
}

bool IsFiniteGroundProfile(const FGroundSurfaceProfile& Profile)
{
    const float Values[] = {
        Profile.RestitutionScale,
        Profile.FrictionScale,
        Profile.ImpactSpinRetention,
        Profile.SkipMinSpeedMps,
        Profile.SkipMaxIncidenceDeg,
        Profile.EdgeRollMinAngleDeg,
        Profile.EdgeRollMinSpeedMps,
        Profile.EdgeRollMinSpinRpm,
        Profile.SlideDecelerationMps2,
        Profile.RollDecelerationMps2,
        Profile.GroundSpinDecayPerSecond,
        Profile.SettleSpeedMps
    };
    for (const float Value : Values)
    {
        if (!FMath::IsFinite(Value) || Value < 0.0f)
        {
            return false;
        }
    }
    return Profile.MaxConsecutiveSkips >= 0;
}

bool IsKnownBasketContactResult(EBasketContactResult Result)
{
    switch (Result)
    {
        case EBasketContactResult::None:
        case EBasketContactResult::Caught:
        case EBasketContactResult::ChainDeflection:
        case EBasketContactResult::BandRejection:
        case EBasketContactResult::TrayRejection:
            return true;
        default:
            return false;
    }
}

bool IsKnownGroundState(EDiscGroundState State)
{
    switch (State)
    {
        case EDiscGroundState::Airborne:
        case EDiscGroundState::Impact:
        case EDiscGroundState::Skipping:
        case EDiscGroundState::Sliding:
        case EDiscGroundState::EdgeRolling:
        case EDiscGroundState::Settled:
            return true;
        default:
            return false;
    }
}

bool IsKnownFlightState(EDiscFlightState State)
{
    switch (State)
    {
        case EDiscFlightState::Idle:
        case EDiscFlightState::Flying:
        case EDiscFlightState::GroundPlay:
        case EDiscFlightState::Settled:
        case EDiscFlightState::HoledOut:
            return true;
        default:
            return false;
    }
}

bool IsKnownGroundSurface(EGroundSurfaceType Surface)
{
    switch (Surface)
    {
        case EGroundSurfaceType::Fairway:
        case EGroundSurfaceType::Rough:
        case EGroundSurfaceType::Dirt:
        case EGroundSurfaceType::Rock:
        case EGroundSurfaceType::TeePad:
            return true;
        default:
            return false;
    }
}

bool IsKnownCourseSurface(ECourseSurfaceType Surface)
{
    switch (Surface)
    {
        case ECourseSurfaceType::Fairway:
        case ECourseSurfaceType::TeePad:
        case ECourseSurfaceType::LightRough:
        case ECourseSurfaceType::DeepRough:
        case ECourseSurfaceType::Dirt:
        case ECourseSurfaceType::Rock:
        case ECourseSurfaceType::OutOfBounds:
        case ECourseSurfaceType::Hazard:
            return true;
        default:
            return false;
    }
}

bool IsKnownFixtureType(EDiscGolfFixtureType FixtureType)
{
    switch (FixtureType)
    {
        case EDiscGolfFixtureType::Unknown:
        case EDiscGolfFixtureType::Tree:
        case EDiscGolfFixtureType::DenseGrass:
        case EDiscGolfFixtureType::Rock:
        case EDiscGolfFixtureType::Sign:
            return true;
        default:
            return false;
    }
}

const TCHAR* GroundStateName(EDiscGroundState State)
{
    switch (State)
    {
        case EDiscGroundState::Skipping: return TEXT("Skip");
        case EDiscGroundState::Sliding: return TEXT("Slide");
        case EDiscGroundState::EdgeRolling: return TEXT("EdgeRoll");
        case EDiscGroundState::Settled: return TEXT("Settled");
        case EDiscGroundState::Impact: return TEXT("Impact");
        case EDiscGroundState::Airborne:
        default: return TEXT("Airborne");
    }
}

const TCHAR* GroundSurfaceName(EGroundSurfaceType Surface)
{
    switch (Surface)
    {
        case EGroundSurfaceType::Rough: return TEXT("Rough");
        case EGroundSurfaceType::Dirt: return TEXT("Dirt");
        case EGroundSurfaceType::Rock: return TEXT("Rock");
        case EGroundSurfaceType::TeePad: return TEXT("TeePad");
        case EGroundSurfaceType::Fairway:
        default: return TEXT("Fairway");
    }
}
}

UDiscFlightComponent::UDiscFlightComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UDiscFlightComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const AActor* Owner = GetOwner())
    {
        BodyToWorld = Owner->GetActorQuat();
        DiscNormalWorld = BodyToWorld.RotateVector(FVector::UpVector).GetSafeNormal();
        DiscForwardWorld = BodyToWorld.RotateVector(FVector::ForwardVector).GetSafeNormal();
    }
}

bool UDiscFlightComponent::ConfigureDisc(const FResolvedDiscDefinition& InDisc)
{
    if (IsFlying())
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc configuration rejected while authoritative flight is active."));
        return false;
    }

    FString Error;
    if (!DiscGolfMath::IsResolvedDiscDefinitionValid(InDisc, &Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc configuration rejected: %s"), *Error);
        return false;
    }
    Disc = InDisc;
    bDiscConfigured = true;
    return true;
}

bool UDiscFlightComponent::SetWindDirector(AWindDirector* InWindDirector)
{
    if (IsFlying())
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Wind director reassignment rejected while authoritative flight is active."));
        return false;
    }
    if (!InWindDirector)
    {
        WindDirector = nullptr;
        return true;
    }

    FString Error;
    if (!InWindDirector->ValidatePhysicsWindConfiguration(Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc flight rejected an invalid wind director: %s"), *Error);
        return false;
    }
    WindDirector = InWindDirector;
    return true;
}

bool UDiscFlightComponent::ValidateSimulationConfiguration(FString& OutError) const
{
    struct FSettingRange
    {
        const TCHAR* Name;
        float Value;
        float Minimum;
        float Maximum;
    };
    const FSettingRange Settings[] = {
        { TEXT("AirDensityKgM3"), AirDensityKgM3,
            MinimumAirDensityKgM3, MaximumAirDensityKgM3 },
        { TEXT("FixedStepSeconds"), FixedStepSeconds,
            MinimumFixedStepSeconds, MaximumFixedStepSeconds },
        { TEXT("MaxFlightSeconds"), MaxFlightSeconds,
            MinimumFlightDurationSeconds, MaximumFlightDurationSeconds },
        { TEXT("MaxGroundPlaySeconds"), MaxGroundPlaySeconds,
            MinimumGroundPlayDurationSeconds, MaximumGroundPlayDurationSeconds },
        { TEXT("MaxPrecessionRateRadPerSec"), MaxPrecessionRateRadPerSec,
            MinimumPrecessionRateRadPerSec, MaximumPrecessionRateRadPerSec },
        { TEXT("GroundContactBiasMps"), GroundContactBiasMps,
            0.0f, MaximumGroundContactBiasMps },
        { TEXT("GroundProbeDepthCm"), GroundProbeDepthCm,
            MinimumGroundProbeDepthCm, MaximumGroundProbeDepthCm },
        { TEXT("TrajectorySampleHz"), TrajectorySampleHz,
            MinimumTrajectorySampleHz, MaximumTrajectorySampleHz }
    };
    for (const FSettingRange& Setting : Settings)
    {
        if (!FMath::IsFinite(Setting.Value)
            || Setting.Value < Setting.Minimum
            || Setting.Value > Setting.Maximum)
        {
            OutError = FString::Printf(
                TEXT("%s must be finite and in [%.6g, %.6g]"),
                Setting.Name, Setting.Minimum, Setting.Maximum);
            return false;
        }
    }
    if (TrajectorySampleHz * FixedStepSeconds > 1.0f + KINDA_SMALL_NUMBER)
    {
        OutError = TEXT("TrajectorySampleHz cannot exceed the fixed-step simulation rate");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UDiscFlightComponent::IsSpawnTransformValidForLaunch(const FTransform& Transform)
{
    const FVector Scale = Transform.GetScale3D();
    return IsFiniteVector(Transform.GetLocation())
        && IsFiniteVector(Scale)
        && Scale.Equals(FVector::OneVector, KINDA_SMALL_NUMBER)
        && IsFiniteQuaternion(Transform.GetRotation());
}

bool UDiscFlightComponent::IsOwnerTransformValidForLaunch(const FTransform& Transform)
{
    const FVector Scale = Transform.GetScale3D();
    return IsFiniteVector(Transform.GetLocation())
        && IsFiniteVector(Scale)
        // ADiscActor's root is the swept cylinder itself. Its canonical scale
        // produces a 21 cm diameter, 1.5 cm thick collision body. Any other
        // root scale changes authoritative contact physics without appearing
        // in the immutable release or trajectory provenance.
        && Scale.Equals(
            DiscGolfMath::CanonicalDiscCollisionScale(), KINDA_SMALL_NUMBER)
        && IsFiniteQuaternion(Transform.GetRotation());
}

bool UDiscFlightComponent::IsContactNormalValid(const FVector& Normal)
{
    return IsFiniteVector(Normal) && Normal.SizeSquared() > SMALL_NUMBER;
}

bool UDiscFlightComponent::ValidateLaunchSimulationConfiguration(FString& OutError) const
{
    if (!LaunchSimulationConfiguration.bCaptured)
    {
        OutError = TEXT("launch simulation configuration was not captured");
        return false;
    }

    struct FSettingPair
    {
        const TCHAR* Name;
        float Current;
        float Accepted;
    };
    const FSettingPair Settings[] = {
        { TEXT("AirDensityKgM3"), AirDensityKgM3,
            LaunchSimulationConfiguration.AirDensityKgM3 },
        { TEXT("FixedStepSeconds"), FixedStepSeconds,
            LaunchSimulationConfiguration.FixedStepSeconds },
        { TEXT("MaxFlightSeconds"), MaxFlightSeconds,
            LaunchSimulationConfiguration.MaxFlightSeconds },
        { TEXT("MaxGroundPlaySeconds"), MaxGroundPlaySeconds,
            LaunchSimulationConfiguration.MaxGroundPlaySeconds },
        { TEXT("MaxPrecessionRateRadPerSec"), MaxPrecessionRateRadPerSec,
            LaunchSimulationConfiguration.MaxPrecessionRateRadPerSec },
        { TEXT("GroundContactBiasMps"), GroundContactBiasMps,
            LaunchSimulationConfiguration.GroundContactBiasMps },
        { TEXT("GroundProbeDepthCm"), GroundProbeDepthCm,
            LaunchSimulationConfiguration.GroundProbeDepthCm },
        { TEXT("TrajectorySampleHz"), TrajectorySampleHz,
            LaunchSimulationConfiguration.TrajectorySampleHz }
    };
    for (const FSettingPair& Setting : Settings)
    {
        if (Setting.Current != Setting.Accepted)
        {
            OutError = FString::Printf(
                TEXT("%s changed after authoritative launch"), Setting.Name);
            return false;
        }
    }

    const AWindDirector* CurrentWindDirector = WindDirector.Get();
    if (!LaunchSimulationConfiguration.bHasPhysicsWindDirector)
    {
        if (CurrentWindDirector)
        {
            OutError = TEXT("physics wind director changed after authoritative launch");
            return false;
        }
    }
    else
    {
        if (!IsValid(CurrentWindDirector)
            || LaunchSimulationConfiguration.PhysicsWindDirector.Get()
                != CurrentWindDirector)
        {
            OutError = TEXT("physics wind director changed or became unavailable after authoritative launch");
            return false;
        }
        if (!CurrentWindDirector->MatchesPhysicsWindConfiguration(
            LaunchSimulationConfiguration.PhysicsWindConfiguration, OutError))
        {
            return false;
        }
    }

    OutError.Reset();
    return true;
}

bool UDiscFlightComponent::ValidateCurrentFlightState(FString& OutError) const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !IsOwnerTransformValidForLaunch(Owner->GetActorTransform()))
    {
        OutError = TEXT("owner transform is missing or non-finite");
        return false;
    }
    if (State != EDiscFlightState::Idle
        && (!bHasAuthoritativeOwnerTransform
            || !Owner->GetActorTransform().Equals(
                LastAuthoritativeOwnerTransform, KINDA_SMALL_NUMBER)))
    {
        OutError = TEXT("owner transform changed outside authoritative flight integration");
        return false;
    }
    if (!bDiscConfigured || !DiscGolfMath::IsResolvedDiscDefinitionValid(Disc, &OutError))
    {
        if (OutError.IsEmpty()) OutError = TEXT("resolved disc is not configured");
        return false;
    }
    if (State != EDiscFlightState::Idle
        && !DiscGolfMath::IsThrowReleaseValid(Telemetry.Release, &OutError))
    {
        return false;
    }
    if (State != EDiscFlightState::Idle
        && !ValidateLaunchSimulationConfiguration(OutError))
    {
        return false;
    }
    if (!IsFiniteVector(VelocityMps)
        || !IsFiniteVector(DiscNormalWorld)
        || !IsFiniteVector(DiscForwardWorld)
        || !IsFiniteVector(GroundNormalWorld)
        || !IsFiniteVector(LaunchWorldLocation)
        || !IsFiniteVector(LastValidPhysicsWindMps)
        || !IsFiniteQuaternion(BodyToWorld)
        || !FMath::IsFinite(SpinRateRadPerSec)
        || !FMath::IsFinite(Accumulator)
        || Accumulator < 0.0)
    {
        OutError = TEXT("solver vectors, attitude, spin, or accumulator are invalid");
        return false;
    }
    const double MaximumBacklogSeconds =
        static_cast<double>(FixedStepSeconds) * MaximumFixedStepsPerTick;
    const FVector DisplacementFromLaunchCm =
        Owner->GetActorLocation() - LaunchWorldLocation;
    const float MaximumSpinRateRadPerSec =
        DiscGolfMath::ThrowReleaseMaximumSpinRpm * 2.0f * PI / 60.0f;
    if (!IsFiniteVector(DisplacementFromLaunchCm)
        || DisplacementFromLaunchCm.Size() > MaximumRuntimeDisplacementCm
        || VelocityMps.Size() > MaximumRuntimeVelocityMps
        || LastValidPhysicsWindMps.Size() > MaximumRuntimeWindMps
        || FMath::Abs(SpinRateRadPerSec) > MaximumSpinRateRadPerSec + KINDA_SMALL_NUMBER
        || Accumulator > MaximumBacklogSeconds + UE_DOUBLE_SMALL_NUMBER)
    {
        OutError = TEXT("solver velocity, spin, wind, displacement, or backlog exceeds its runtime envelope");
        return false;
    }
    if (DiscNormalWorld.SizeSquared() < SMALL_NUMBER
        || DiscForwardWorld.SizeSquared() < SMALL_NUMBER
        || GroundNormalWorld.SizeSquared() < SMALL_NUMBER
        || !IsKnownFlightState(State)
        || !IsKnownGroundState(GroundState)
        || !IsKnownGroundSurface(GroundSurface)
        || !IsKnownCourseSurface(CourseSurface)
        || CurrentGroundProfile.Surface != GroundSurface
        || !IsFiniteGroundProfile(CurrentGroundProfile))
    {
        OutError = TEXT("solver normals or ground profile are invalid");
        return false;
    }
    const bool bStatePairValid =
        (State == EDiscFlightState::Idle && GroundState == EDiscGroundState::Airborne)
        || (State == EDiscFlightState::Flying
            && (GroundState == EDiscGroundState::Airborne
                || GroundState == EDiscGroundState::Skipping))
        || (State == EDiscFlightState::GroundPlay
            && (GroundState == EDiscGroundState::Sliding
                || GroundState == EDiscGroundState::EdgeRolling))
        || (State == EDiscFlightState::Settled && GroundState == EDiscGroundState::Settled)
        || (State == EDiscFlightState::HoledOut && GroundState == EDiscGroundState::Settled);
    if (!bStatePairValid
        || Telemetry.State != State
        || Telemetry.GroundState != GroundState
        || Telemetry.GroundSurface != GroundSurface
        || Telemetry.CourseSurface != CourseSurface)
    {
        OutError = TEXT("flight, ground, and telemetry state are inconsistent");
        return false;
    }

    const float TelemetryScalars[] = {
        Telemetry.SpeedMps,
        Telemetry.SpinRpm,
        Telemetry.AngleOfAttackDeg,
        Telemetry.FlightTimeSeconds,
        Telemetry.WindPhaseOriginSeconds,
        Telemetry.CarryMeters,
        Telemetry.GroundPlayTimeSeconds,
        Telemetry.GroundDistanceMeters,
        Telemetry.LastImpactSpeedMps,
        Telemetry.LastImpactIncidenceDeg,
        Telemetry.LastDiscEdgeAngleDeg,
        Telemetry.LastFixtureImpactSpeedMps,
        Telemetry.LastFixtureEntrySpinRpm,
        Telemetry.LastFixtureExitSpinRpm
    };
    for (const float Value : TelemetryScalars)
    {
        if (!FMath::IsFinite(Value))
        {
            OutError = TEXT("flight telemetry contains a non-finite scalar");
            return false;
        }
    }
    if (!IsFiniteVector(Telemetry.VelocityMps)
        || !IsFiniteVector(Telemetry.LastFixtureEntryVelocityMps)
        || !IsFiniteVector(Telemetry.LastFixtureExitVelocityMps)
        || !IsFiniteVector(Telemetry.LastFixtureImpactNormal)
        || !IsKnownFlightState(Telemetry.State)
        || !IsKnownGroundState(Telemetry.GroundState)
        || !IsKnownGroundSurface(Telemetry.GroundSurface)
        || !IsKnownCourseSurface(Telemetry.CourseSurface)
        || !IsKnownFixtureType(Telemetry.LastFixtureType)
        || !IsKnownBasketContactResult(Telemetry.LastBasketContact)
        || Telemetry.FlightTimeSeconds < 0.0f
        || Telemetry.FlightTimeSeconds > DiscGolfMath::EffectiveFlightTimeoutSeconds(
            Telemetry.Release.ShotContext, Disc.Speed, MaxFlightSeconds)
            + FixedStepSeconds + KINDA_SMALL_NUMBER
        || Telemetry.WindPhaseOriginSeconds != Telemetry.Release.WindPhaseOriginSeconds
        || Telemetry.CarryMeters < 0.0f
        || Telemetry.CarryMeters > MaximumRuntimeDisplacementCm / 100.0f
        || Telemetry.GroundPlayTimeSeconds < 0.0f
        || Telemetry.GroundPlayTimeSeconds > Telemetry.FlightTimeSeconds + KINDA_SMALL_NUMBER
        || Telemetry.GroundPlayTimeSeconds > MaxGroundPlaySeconds + FixedStepSeconds + KINDA_SMALL_NUMBER
        || Telemetry.GroundDistanceMeters < 0.0f
        || Telemetry.SpeedMps < 0.0f
        || Telemetry.SpeedMps > MaximumRuntimeVelocityMps
        || Telemetry.SpinRpm < 0.0f
        || Telemetry.SpinRpm > DiscGolfMath::ThrowReleaseMaximumSpinRpm + KINDA_SMALL_NUMBER
        || FMath::Abs(Telemetry.AngleOfAttackDeg) > 180.0f
        || Telemetry.LastImpactSpeedMps < 0.0f
        || Telemetry.LastImpactIncidenceDeg < 0.0f
        || Telemetry.LastDiscEdgeAngleDeg < 0.0f
        || Telemetry.LastFixtureImpactSpeedMps < 0.0f
        || Telemetry.LastFixtureEntrySpinRpm < 0.0f
        || Telemetry.LastFixtureExitSpinRpm < 0.0f
        || Telemetry.VelocityMps.Size() > MaximumRuntimeVelocityMps
        || Telemetry.LastFixtureEntryVelocityMps.Size() > MaximumRuntimeVelocityMps
        || Telemetry.LastFixtureExitVelocityMps.Size() > MaximumRuntimeVelocityMps
        || Telemetry.GroundContactCount < 0
        || Telemetry.GroundContactCount > MaximumRuntimeContactCount
        || Telemetry.FixtureContactCount < 0
        || Telemetry.FixtureContactCount > MaximumRuntimeContactCount
        || Telemetry.BasketContactCount < 0
        || Telemetry.BasketContactCount > MaximumRuntimeContactCount)
    {
        OutError = TEXT("flight telemetry vectors, clocks, or counters are invalid");
        return false;
    }
    OutError.Reset();
    return true;
}

void UDiscFlightComponent::TerminateInvalidFlight(
    const FString& Context,
    const FString& Error)
{
    bLastFlightTerminatedInvalidly = true;
    LastFlightValidationFailureContext = Context;
    LastFlightValidationFailure = Error;
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("Authoritative flight terminated at %s: %s"), *Context, *Error);
    if (!IsFlying())
    {
        return;
    }

    FString StateError;
    if (ValidateCurrentFlightState(StateError))
    {
        StopFlight(true);
        return;
    }

    AActor* Owner = GetOwner();
    if (Owner
        && bHasAuthoritativeOwnerTransform
        && IsOwnerTransformValidForLaunch(LastAuthoritativeOwnerTransform))
    {
        Owner->SetActorTransform(
            LastAuthoritativeOwnerTransform,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
    }

    const FThrowRelease Release = Telemetry.Release;
    const bool bReleaseValid = DiscGolfMath::IsThrowReleaseValid(Release);
    VelocityMps = FVector::ZeroVector;
    SpinRateRadPerSec = 0.0f;
    Accumulator = 0.0;
    State = EDiscFlightState::Settled;
    GroundState = EDiscGroundState::Settled;
    Telemetry = FDiscFlightTelemetry();
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
    if (bReleaseValid)
    {
        Telemetry.Release = Release;
        Telemetry.WindPhaseOriginSeconds = Release.WindPhaseOriginSeconds;
    }
    OnFlightSettled.Broadcast(Telemetry);
}

FVector UDiscFlightComponent::SampleWindForFixedStep(
    const AWindDirector* InWindDirector,
    const FVector& WorldLocation,
    float SimulationTimeSeconds)
{
    FVector WindMps = FVector::ZeroVector;
    FString Error;
    return TrySampleWindForFixedStep(
        InWindDirector, WorldLocation, SimulationTimeSeconds, WindMps, Error)
        ? WindMps
        : FVector::ZeroVector;
}

bool UDiscFlightComponent::TrySampleWindForFixedStep(
    const AWindDirector* InWindDirector,
    const FVector& WorldLocation,
    float SimulationTimeSeconds,
    FVector& OutWindMps,
    FString& OutError)
{
    OutWindMps = FVector::ZeroVector;
    const bool bLocationFinite = FMath::IsFinite(WorldLocation.X)
        && FMath::IsFinite(WorldLocation.Y)
        && FMath::IsFinite(WorldLocation.Z);
    if (!bLocationFinite
        || !FMath::IsFinite(SimulationTimeSeconds)
        || SimulationTimeSeconds < 0.0f)
    {
        OutError = TEXT("Fixed-step wind sample location/time is invalid");
        return false;
    }
    if (!InWindDirector)
    {
        OutError.Reset();
        return true;
    }
    return InWindDirector->TryGetWindMpsAtSimulationTime(
        WorldLocation, SimulationTimeSeconds, OutWindMps, OutError);
}

bool UDiscFlightComponent::Launch(const FThrowRelease& Release)
{
    AActor* Owner = GetOwner();
    FString Error;
    if (!Owner || !IsOwnerTransformValidForLaunch(Owner->GetActorTransform()))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because the owner transform is missing or invalid."));
        return false;
    }
    if (IsFlying())
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because authoritative flight is already active."));
        return false;
    }
    if (!bDiscConfigured
        || !DiscGolfMath::IsResolvedDiscDefinitionValid(Disc, &Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because its resolved disc is invalid: %s"),
            Error.IsEmpty() ? TEXT("disc was never configured") : *Error);
        return false;
    }
    if (!DiscGolfMath::IsThrowReleaseValid(Release, &Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because its release is invalid: %s"), *Error);
        return false;
    }
    if (!ValidateSimulationConfiguration(Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because its simulation configuration is invalid: %s"),
            *Error);
        return false;
    }

    AWindDirector* CandidateWindDirector = WindDirector.Get();
    const bool bHasCandidateWindDirector = IsValid(CandidateWindDirector);
    if (CandidateWindDirector && !bHasCandidateWindDirector)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because its physics wind director is unavailable."));
        return false;
    }
    FDiscGolfPhysicsWindConfigurationSnapshot CandidateWindConfiguration;
    if (bHasCandidateWindDirector
        && !CandidateWindDirector->TryCapturePhysicsWindConfiguration(
            CandidateWindConfiguration, Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because its physics wind configuration could not be frozen: %s"),
            *Error);
        return false;
    }

    FVector InitialWindMps = FVector::ZeroVector;
    if (!TrySampleWindForFixedStep(
        CandidateWindDirector, Owner->GetActorLocation(), Release.WindPhaseOriginSeconds,
        InitialWindMps, Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because the physics wind sample is invalid: %s"),
            *Error);
        return false;
    }

    FVector FlatDirection(Release.Direction.X, Release.Direction.Y, 0.0f);
    FlatDirection = FlatDirection.GetSafeNormal();
    const FVector WorldUp = FVector::UpVector;
    FVector Right = FVector::CrossProduct(WorldUp, FlatDirection).GetSafeNormal();
    const FQuat AimError(WorldUp, FMath::DegreesToRadians(Release.AimOffsetDeg));
    FlatDirection = AimError.RotateVector(FlatDirection).GetSafeNormal();
    Right = FVector::CrossProduct(WorldUp, FlatDirection).GetSafeNormal();

    const FVector LaunchDirection = DiscGolfMath::LaunchDirectionFromFlat(
        FlatDirection, Release.EffectiveLaunchAngleDeg);
    const FVector InitialVelocityMps = LaunchDirection * Release.ReleaseSpeedMps;

    const float RotationSign = DiscGolfMath::ThrowRotationSign(
        Release.ThrowStyle, Release.Handedness);

    // Nose angle is defined relative to the launch trajectory, not the horizon.
    // Positive nose means the leading edge is above the trajectory.
    const FQuat NoseRotation(Right, FMath::DegreesToRadians(-Release.EffectiveNoseAngleDeg));
    const FVector InitialDiscForwardWorld =
        NoseRotation.RotateVector(LaunchDirection).GetSafeNormal();
    FVector InitialDiscNormalWorld =
        FVector::CrossProduct(InitialDiscForwardWorld, Right).GetSafeNormal();

    // Positive hyzer means the outside edge is down. Throw style and handedness
    // together determine the world-space bank direction.
    const FQuat HyzerRotation(InitialDiscForwardWorld,
        FMath::DegreesToRadians(RotationSign * Release.EffectiveHyzerDeg));
    InitialDiscNormalWorld =
        HyzerRotation.RotateVector(InitialDiscNormalWorld).GetSafeNormal();
    const FQuat InitialBodyToWorld = FRotationMatrix::MakeFromXZ(
        InitialDiscForwardWorld, InitialDiscNormalWorld).ToQuat().GetNormalized();
    const float InitialSpinRateRadPerSec =
        RotationSign * Release.SpinRpm * 2.0f * PI / 60.0f;
    const FGroundSurfaceProfile InitialGroundProfile =
        DiscGolfMath::GroundSurfaceProfile(EGroundSurfaceType::Fairway);

    // Reject individually finite but unsafe combinations (for example an
    // extreme coefficient/area profile paired with a high release or wind)
    // before the launch transaction can commit a stroke and before any world
    // movement occurs. This mirrors the first airborne force calculation.
    const FVector InitialAirVelocityMps = InitialVelocityMps - InitialWindMps;
    const float InitialAirSpeedMps = InitialAirVelocityMps.Size();
    const FVector InitialAirDirection = InitialAirSpeedMps > SMALL_NUMBER
        ? InitialAirVelocityMps / InitialAirSpeedMps
        : FVector::ZeroVector;
    const float InitialNormalAirSpeedMps = FVector::DotProduct(
        InitialAirVelocityMps, InitialDiscNormalWorld);
    const FVector InitialAirVelocityInPlane = InitialAirVelocityMps
        - InitialNormalAirSpeedMps * InitialDiscNormalWorld;
    const float InitialPlaneAirSpeedMps = FMath::Max(
        InitialAirVelocityInPlane.Size(), 1.0e-4f);
    const float InitialAlphaRad = -FMath::Atan2(
        InitialNormalAirSpeedMps, InitialPlaneAirSpeedMps);
    FVector InitialUPlane = InitialAirVelocityInPlane.GetSafeNormal();
    if (InitialUPlane.IsNearlyZero()) InitialUPlane = InitialDiscForwardWorld;
    FVector InitialULateral = FVector::CrossProduct(
        InitialDiscNormalWorld, InitialUPlane).GetSafeNormal();
    if (InitialULateral.IsNearlyZero()) InitialULateral = FVector::RightVector;
    const FVector InitialLiftDirection = FVector::CrossProduct(
        InitialAirDirection, InitialULateral).GetSafeNormal();
    const float InitialAlphaEq = -Disc.Aero.CL0 / FMath::Max(Disc.Aero.CLa, 1.0e-4f);
    const float InitialCL = FMath::Clamp(
        Disc.Aero.CL0 + Disc.Aero.CLa * InitialAlphaRad, -0.8f, 1.7f);
    const float InitialCD = FMath::Clamp(
        Disc.Aero.CD0 + Disc.Aero.CDa * FMath::Square(InitialAlphaRad - InitialAlphaEq),
        0.02f, 1.2f);
    const float InitialDynamicPressureArea = 0.5f * AirDensityKgM3
        * Disc.Aero.AreaM2 * InitialAirSpeedMps * InitialAirSpeedMps;
    const FVector InitialAeroForceN = InitialLiftDirection
        * (InitialCL * InitialDynamicPressureArea)
        - InitialAirDirection * (InitialCD * InitialDynamicPressureArea);
    const FVector InitialGravityN(0.0f, 0.0f, -Disc.Aero.MassKg * 9.80665f);
    const FVector InitialAccelerationMps2 = (InitialAeroForceN + InitialGravityN)
        / Disc.Aero.MassKg;

    if (!IsFiniteVector(InitialVelocityMps)
        || !IsFiniteVector(InitialDiscForwardWorld)
        || InitialDiscForwardWorld.SizeSquared() < SMALL_NUMBER
        || !IsFiniteVector(InitialDiscNormalWorld)
        || InitialDiscNormalWorld.SizeSquared() < SMALL_NUMBER
        || !IsFiniteQuaternion(InitialBodyToWorld)
        || !FMath::IsFinite(InitialSpinRateRadPerSec)
        || !IsFiniteGroundProfile(InitialGroundProfile)
        || !IsFiniteVector(InitialAirVelocityMps)
        || !FMath::IsFinite(InitialAirSpeedMps)
        || !FMath::IsFinite(InitialAlphaRad)
        || !IsFiniteVector(InitialAeroForceN)
        || !IsFiniteVector(InitialAccelerationMps2)
        || InitialAccelerationMps2.Size() > MaximumRuntimeAccelerationMps2)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because its derived initial state is invalid."));
        return false;
    }

    FSimulationConfigurationSnapshot CandidateSimulationConfiguration;
    CandidateSimulationConfiguration.AirDensityKgM3 = AirDensityKgM3;
    CandidateSimulationConfiguration.FixedStepSeconds = FixedStepSeconds;
    CandidateSimulationConfiguration.MaxFlightSeconds = MaxFlightSeconds;
    CandidateSimulationConfiguration.MaxGroundPlaySeconds = MaxGroundPlaySeconds;
    CandidateSimulationConfiguration.MaxPrecessionRateRadPerSec = MaxPrecessionRateRadPerSec;
    CandidateSimulationConfiguration.GroundContactBiasMps = GroundContactBiasMps;
    CandidateSimulationConfiguration.GroundProbeDepthCm = GroundProbeDepthCm;
    CandidateSimulationConfiguration.TrajectorySampleHz = TrajectorySampleHz;
    CandidateSimulationConfiguration.PhysicsWindDirector = CandidateWindDirector;
    CandidateSimulationConfiguration.PhysicsWindConfiguration =
        MoveTemp(CandidateWindConfiguration);
    CandidateSimulationConfiguration.bHasPhysicsWindDirector =
        bHasCandidateWindDirector;
    CandidateSimulationConfiguration.bCaptured = true;

    // All fallible inputs and derived values are known-good before the first
    // actor, flight-state, telemetry, or trajectory mutation occurs.
    const FTransform PreLaunchOwnerTransform = Owner->GetActorTransform();
    if (!Owner->SetActorRotation(InitialBodyToWorld, ETeleportType::TeleportPhysics))
    {
        Owner->SetActorTransform(
            PreLaunchOwnerTransform, false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because its validated launch rotation could not be applied."));
        return false;
    }
    const FTransform AppliedLaunchTransform = Owner->GetActorTransform();
    if (!IsOwnerTransformValidForLaunch(AppliedLaunchTransform)
        || !AppliedLaunchTransform.GetRotation().Equals(
            InitialBodyToWorld, KINDA_SMALL_NUMBER))
    {
        Owner->SetActorTransform(
            PreLaunchOwnerTransform, false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because its authoritative owner rotation was not preserved."));
        return false;
    }

    // Applying the launch rotation can synchronously dispatch extension
    // callbacks while the component is still Idle. Recheck the exact wind
    // authority after that callback boundary so launch never commits a stale
    // director, zone set, or configuration snapshot.
    AWindDirector* PostRotationWindDirector = WindDirector.Get();
    const bool bWindAuthorityUnchanged = bHasCandidateWindDirector
        ? IsValid(PostRotationWindDirector)
            && PostRotationWindDirector == CandidateWindDirector
            && PostRotationWindDirector->MatchesPhysicsWindConfiguration(
                CandidateSimulationConfiguration.PhysicsWindConfiguration, Error)
        : PostRotationWindDirector == nullptr;
    if (!bWindAuthorityUnchanged)
    {
        Owner->SetActorTransform(
            PreLaunchOwnerTransform, false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc launch rejected because physics wind authority changed during launch: %s"),
            Error.IsEmpty() ? TEXT("director identity changed") : *Error);
        return false;
    }

    LaunchWorldLocation = Owner->GetActorLocation();
    LastAuthoritativeOwnerTransform = AppliedLaunchTransform;
    bHasAuthoritativeOwnerTransform = true;
    VelocityMps = InitialVelocityMps;
    DiscForwardWorld = InitialDiscForwardWorld;
    DiscNormalWorld = InitialDiscNormalWorld;
    BodyToWorld = InitialBodyToWorld;
    SpinRateRadPerSec = InitialSpinRateRadPerSec;
    LastValidPhysicsWindMps = InitialWindMps;
    LaunchSimulationConfiguration = CandidateSimulationConfiguration;
    Accumulator = 0.0;
    State = EDiscFlightState::Flying;
    GroundState = EDiscGroundState::Airborne;
    GroundSurface = EGroundSurfaceType::Fairway;
    CourseSurface = ECourseSurfaceType::Fairway;
    CurrentGroundProfile = InitialGroundProfile;
    GroundNormalWorld = FVector::UpVector;
    ConsecutiveSkipCount = 0;
    Telemetry = FDiscFlightTelemetry();
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
    Telemetry.GroundSurface = GroundSurface;
    Telemetry.CourseSurface = CourseSurface;
    Telemetry.Release = Release;
    Telemetry.WindPhaseOriginSeconds = Release.WindPhaseOriginSeconds;
    TrajectorySamples.Reset();
    GroundTransitions.Reset();
    VegetationOverlapTimes.Reset();
    NextTrajectorySampleTime = 0.0f;
    bTrajectorySampleScheduleInitialized = false;
    bLastFlightTerminatedInvalidly = false;
    LastFlightValidationFailureContext.Reset();
    LastFlightValidationFailure.Reset();
    UpdateTelemetry(0.0f);
    RecordTrajectorySample(true);
    return true;
}

void UDiscFlightComponent::StopFlight(bool bNotifySettled)
{
    const bool bWasActive = IsFlying();
    if (!bWasActive)
    {
        return;
    }
    const EDiscGroundState PreviousGroundState = GroundState;
    VelocityMps = FVector::ZeroVector;
    SpinRateRadPerSec = 0.0f;
    Accumulator = 0.0;
    State = EDiscFlightState::Settled;
    GroundState = EDiscGroundState::Settled;
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
    if (Telemetry.GroundContactCount > 0 && PreviousGroundState != EDiscGroundState::Settled)
    {
        RecordGroundTransition(PreviousGroundState, EDiscGroundState::Settled);
    }
    RecordTrajectorySample(true);

    if (Telemetry.GroundContactCount > 0)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("Ground settled on %s after %d impact(s), %.2f m ground travel, %.2f s after first contact."),
            GroundSurfaceName(GroundSurface), Telemetry.GroundContactCount,
            Telemetry.GroundDistanceMeters, Telemetry.GroundPlayTimeSeconds);
    }

    if (bNotifySettled)
    {
        OnFlightSettled.Broadcast(Telemetry);
    }
}

bool UDiscFlightComponent::ApplyBasketContact(
    const FBasketContactEvaluation& Evaluation,
    const FVector& CaptureWorldLocationCm)
{
    if (!IsFlying()
        || !IsKnownBasketContactResult(Evaluation.Result)
        || Evaluation.Result == EBasketContactResult::None)
    {
        return false;
    }
    AActor* Owner = GetOwner();
    const FVector CaptureDeltaCm = Owner
        ? CaptureWorldLocationCm - Owner->GetActorLocation()
        : FVector::ZeroVector;
    const bool bCaptureSafe = Evaluation.Result != EBasketContactResult::Caught
        || (Owner
            && IsFiniteVector(CaptureDeltaCm)
            && CaptureDeltaCm.Size() <= MaximumBasketCaptureSnapCm
            && IsFiniteVector(CaptureWorldLocationCm - LaunchWorldLocation)
            && (CaptureWorldLocationCm - LaunchWorldLocation).Size()
                <= MaximumRuntimeDisplacementCm);
    const bool bEvaluationFinite = FMath::IsFinite(Evaluation.PredictedRadialCm)
        && FMath::IsFinite(Evaluation.PredictedHeightCm)
        && FMath::IsFinite(Evaluation.IncomingSpeedMps)
        && Evaluation.PredictedRadialCm >= 0.0f
        && Evaluation.IncomingSpeedMps >= 0.0f
        && IsFiniteVector(Evaluation.DeflectedVelocityMps)
        && IsFiniteVector(CaptureWorldLocationCm);
    const bool bDeflectionSafe = Evaluation.Result == EBasketContactResult::Caught
        || Evaluation.DeflectedVelocityMps.Size()
            <= MaximumRuntimeVelocityMps;
    if (!bEvaluationFinite || !bDeflectionSafe || !bCaptureSafe)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Basket contact rejected because its evaluation/capture boundary is invalid."));
        return false;
    }

    FString StateError;
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("basket contact preflight"), StateError);
        return false;
    }
    if (Telemetry.BasketContactCount >= MaximumRuntimeContactCount)
    {
        TerminateInvalidFlight(
            TEXT("basket contact saturation"),
            TEXT("basket contact count reached its runtime limit"));
        return false;
    }

    if (Evaluation.Result == EBasketContactResult::Caught)
    {
        bool bCaptureMoveApplied = false;
        bool bCaptureTransformPreserved = false;
        {
            FScopedMovementUpdate ScopedMovement(
                Owner->GetRootComponent(), EScopedUpdate::DeferredUpdates);
            bCaptureMoveApplied = Owner->SetActorLocation(
                CaptureWorldLocationCm, false, nullptr, ETeleportType::TeleportPhysics);
            if (bCaptureMoveApplied)
            {
                const FTransform AppliedCaptureTransform = Owner->GetActorTransform();
                bCaptureTransformPreserved =
                    IsOwnerTransformValidForLaunch(AppliedCaptureTransform)
                    && AppliedCaptureTransform.GetLocation().Equals(
                        CaptureWorldLocationCm, KINDA_SMALL_NUMBER);
                if (bCaptureTransformPreserved)
                {
                    // Deferred overlap callbacks run when this scope closes.
                    // Publish the authoritative snap first so those callbacks
                    // validate against the committed solver-owned transform.
                    LastAuthoritativeOwnerTransform = AppliedCaptureTransform;
                }
            }
            if (!bCaptureTransformPreserved)
            {
                ScopedMovement.RevertMove();
            }
        }
        if (!bCaptureMoveApplied)
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("Basket catch rejected because the validated capture transform could not be applied."));
            return false;
        }
        if (!bCaptureTransformPreserved)
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("Basket contact rejected because the validated capture transform was not preserved."));
            return false;
        }
        if (!ValidateCurrentFlightState(StateError))
        {
            TerminateInvalidFlight(TEXT("basket capture deferred callbacks"), StateError);
            return false;
        }
        if (!IsFlying()) return false;
        ++Telemetry.BasketContactCount;
        Telemetry.LastBasketContact = Evaluation.Result;
        VelocityMps = FVector::ZeroVector;
        UpdateTelemetry(0.0f);
        // A catch snaps the disc from the overlap edge to the authoritative
        // basket capture point without advancing solver time. Discard only the
        // superseded pre-snap sample at that exact timestamp so the replay
        // source retains the later actual state and never invents interpolation
        // across a zero-duration teleport.
        while (!TrajectorySamples.IsEmpty()
            && TrajectorySamples.Last().TimeSeconds == Telemetry.FlightTimeSeconds
            && !TrajectorySamples.Last().WorldLocationCm.Equals(
                CaptureWorldLocationCm, KINDA_SMALL_NUMBER))
        {
            TrajectorySamples.Pop(EAllowShrinking::No);
        }
        RecordTrajectorySample(true);
        return true;
    }

    const float DeflectedSpinRateRadPerSec = SpinRateRadPerSec
        * (Evaluation.Result == EBasketContactResult::ChainDeflection ? 0.42f : 0.68f);
    if (!FMath::IsFinite(DeflectedSpinRateRadPerSec))
    {
        return false;
    }
    ++Telemetry.BasketContactCount;
    Telemetry.LastBasketContact = Evaluation.Result;
    VelocityMps = Evaluation.DeflectedVelocityMps;
    SpinRateRadPerSec = DeflectedSpinRateRadPerSec;
    UpdateTelemetry(0.0f);
    RecordTrajectorySample(true);
    return true;
}

void UDiscFlightComponent::ApplyFixtureOverlap(AActor* FixtureActor)
{
    if (!IsFlying()) return;
    FString StateError;
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("fixture overlap preflight"), StateError);
        return;
    }
    if (const ADiscGolfVegetationInteractionActor* Vegetation =
        Cast<ADiscGolfVegetationInteractionActor>(FixtureActor))
    {
        if (!Vegetation->HasValidInteractionContract()) return;
        // Re-entry is part of the authoritative contact simulation. Use the
        // fixed-step flight clock so identical throws cannot receive a
        // different number of vegetation impulses at different render rates.
        const float Now = Telemetry.FlightTimeSeconds;
        if (!FMath::IsFinite(Now) || Now < 0.0f) return;
        const TWeakObjectPtr<AActor> Key(FixtureActor);
        if (const float* Previous = VegetationOverlapTimes.Find(Key))
        {
            if (Now - *Previous < Vegetation->Profile.ReentryCooldownSeconds) return;
        }
        const FVector ResultVelocityMps = VelocityMps
            * FMath::Clamp(Vegetation->Profile.VelocityMultiplier, 0.05f, 1.0f);
        const float ResultSpinRateRadPerSec = SpinRateRadPerSec
            * FMath::Clamp(Vegetation->Profile.SpinMultiplier, 0.05f, 1.0f);
        if (!IsFiniteVector(ResultVelocityMps)
            || !FMath::IsFinite(ResultSpinRateRadPerSec))
        {
            return;
        }
        if (Telemetry.FixtureContactCount >= MaximumRuntimeContactCount)
        {
            TerminateInvalidFlight(
                TEXT("fixture overlap saturation"),
                TEXT("fixture contact count reached its runtime limit"));
            return;
        }
        VegetationOverlapTimes.Add(Key, Now);
        VelocityMps = ResultVelocityMps;
        SpinRateRadPerSec = ResultSpinRateRadPerSec;
        ++Telemetry.FixtureContactCount;
        UpdateTelemetry(0.0f);
        RecordTrajectorySample(true);
        return;
    }
    const ADiscGolfWorldFixtureActor* Fixture = Cast<ADiscGolfWorldFixtureActor>(FixtureActor);
    if (!Fixture || !Fixture->IsOverlapFixture()) return;

    const FVector EntryVelocityMps = VelocityMps;
    const float EntrySpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
    const FDiscGolfFixtureImpactResult Result = DiscGolfMath::ResolveFixtureOverlap(
        EntryVelocityMps, Fixture->FixtureType);
    const float ResultSpinRateRadPerSec = SpinRateRadPerSec * Result.SpinMultiplier;
    if (!IsFiniteVector(Result.VelocityMps)
        || Result.VelocityMps.Size() > MaximumRuntimeVelocityMps
        || !FMath::IsFinite(Result.SpinMultiplier)
        || Result.SpinMultiplier < 0.0f
        || !FMath::IsFinite(Result.ImpactSpeedMps)
        || Result.ImpactSpeedMps < 0.0f
        || !FMath::IsFinite(Result.NormalSpeedMps)
        || Result.NormalSpeedMps < 0.0f
        || !FMath::IsFinite(ResultSpinRateRadPerSec))
    {
        return;
    }
    if (Telemetry.FixtureContactCount >= MaximumRuntimeContactCount)
    {
        TerminateInvalidFlight(
            TEXT("fixture overlap saturation"),
            TEXT("fixture contact count reached its runtime limit"));
        return;
    }
    VelocityMps = Result.VelocityMps;
    SpinRateRadPerSec = ResultSpinRateRadPerSec;
    ++Telemetry.FixtureContactCount;
    Telemetry.LastFixtureType = Result.FixtureType;
    Telemetry.LastFixtureImpactSpeedMps = Result.ImpactSpeedMps;
    Telemetry.LastFixtureEntryVelocityMps = EntryVelocityMps;
    Telemetry.LastFixtureExitVelocityMps = VelocityMps;
    Telemetry.LastFixtureImpactNormal = FVector::ZeroVector;
    Telemetry.LastFixtureEntrySpinRpm = EntrySpinRpm;
    Telemetry.LastFixtureExitSpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
    Telemetry.LastImpactSpeedMps = Result.ImpactSpeedMps;
    UpdateTelemetry(Telemetry.AngleOfAttackDeg * PI / 180.0f);
    RecordTrajectorySample(true);

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Fixture overlap %s: speed %.2f -> %.2f m/s, spin retention %.2f."),
        *Fixture->FixtureId.ToString(), Result.ImpactSpeedMps, Result.VelocityMps.Size(), Result.SpinMultiplier);
}

void UDiscFlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!IsFlying()) return;

    if (!FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f)
    {
        TerminateInvalidFlight(
            TEXT("tick delta"), TEXT("render delta must be finite and non-negative"));
        return;
    }
    FString Error;
    if (!ValidateSimulationConfiguration(Error))
    {
        TerminateInvalidFlight(TEXT("tick configuration"), Error);
        return;
    }
    if (!ValidateCurrentFlightState(Error))
    {
        TerminateInvalidFlight(TEXT("tick state"), Error);
        return;
    }
    if (DeltaTime == 0.0f) return;

    const double MaximumBacklogSeconds =
        static_cast<double>(FixedStepSeconds) * MaximumFixedStepsPerTick;
    Accumulator = FMath::Min(
        Accumulator + FMath::Min<double>(DeltaTime, MaximumAcceptedTickDeltaSeconds),
        MaximumBacklogSeconds);
    int32 Steps = 0;
    while (Accumulator >= FixedStepSeconds
        && Steps < MaximumFixedStepsPerTick
        && IsFlying())
    {
        Accumulator -= FixedStepSeconds;
        ++Steps;
        SimulateFixedStep(FixedStepSeconds);
    }
    if (Accumulator < 0.0 && Accumulator > -UE_DOUBLE_SMALL_NUMBER)
    {
        Accumulator = 0.0;
    }
}

FVector UDiscFlightComponent::ComputeAeroForceN(const FVector& AirVelocityMps, float& OutAlphaRad, FVector& OutUPlane, FVector& OutULateral) const
{
    const float Speed = AirVelocityMps.Size();
    if (Speed < 0.05f) return FVector::ZeroVector;

    const FVector UVelocity = AirVelocityMps / Speed;
    const float NormalSpeed = FVector::DotProduct(AirVelocityMps, DiscNormalWorld);
    const FVector VelocityInPlane = AirVelocityMps - NormalSpeed * DiscNormalWorld;
    const float PlaneSpeed = FMath::Max(VelocityInPlane.Size(), 1.0e-4f);

    // AirVelocity is the disc's motion through air. The sign is inverted so nose-up is positive AoA.
    OutAlphaRad = -FMath::Atan2(NormalSpeed, PlaneSpeed);
    OutUPlane = VelocityInPlane.GetSafeNormal();
    if (OutUPlane.IsNearlyZero()) OutUPlane = DiscForwardWorld;
    OutULateral = FVector::CrossProduct(DiscNormalWorld, OutUPlane).GetSafeNormal();
    if (OutULateral.IsNearlyZero()) OutULateral = FVector::RightVector;

    // With UVelocity forward and ULateral right, cross(forward,right) points toward lift/up.
    const FVector LiftDirection = FVector::CrossProduct(UVelocity, OutULateral).GetSafeNormal();
    const float AlphaEq = -Disc.Aero.CL0 / FMath::Max(Disc.Aero.CLa, 1.0e-4f);
    const float CL = FMath::Clamp(Disc.Aero.CL0 + Disc.Aero.CLa * OutAlphaRad, -0.8f, 1.7f);
    const float CD = FMath::Clamp(Disc.Aero.CD0 + Disc.Aero.CDa * FMath::Square(OutAlphaRad - AlphaEq), 0.02f, 1.2f);
    const float DynamicPressureArea = 0.5f * AirDensityKgM3 * Disc.Aero.AreaM2 * Speed * Speed;

    return LiftDirection * (CL * DynamicPressureArea) - UVelocity * (CD * DynamicPressureArea);
}

FVector UDiscFlightComponent::ComputeAeroTorqueWorldNm(const FVector& AirVelocityMps, float AlphaRad, const FVector& UPlane, const FVector& ULateral) const
{
    const float Speed = AirVelocityMps.Size();
    if (Speed < 0.05f) return FVector::ZeroVector;

    const float QAd = 0.5f * AirDensityKgM3 * Disc.Aero.AreaM2 * Speed * Speed * Disc.Aero.DiameterM;

    // Keep the trim moment available for later measured-data fitting, but neutral by default in v0.1.
    const float TrimMomentNm = QAd * (Disc.Aero.CM0 + Disc.Aero.CMa * AlphaRad);

    // The signed spin in IntegrateAttitude automatically mirrors RHBH/RHFH precession.
    // For positive-spin RHBH, turn and fade intentionally oppose one another.
    const float StabilityMomentNm = DiscGolfMath::DiscStabilityMomentNm(
        Speed, Disc.Aero.HighSpeedTurnMomentNm, Disc.Aero.LowSpeedFadeMomentNm,
        Disc.Aero.TurnStartsAboveMps, Disc.Aero.FadeStartsBelowMps);
    return UPlane * TrimMomentNm + ULateral * StabilityMomentNm;
}

void UDiscFlightComponent::IntegrateAttitude(const FVector& TorqueWorldNm, float Dt)
{
    const float SignedAngularMomentum = Disc.Aero.InertiaAxialKgM2 * SpinRateRadPerSec;
    if (FMath::Abs(SignedAngularMomentum) > 1.0e-5f)
    {
        const FVector TorquePerpendicular = TorqueWorldNm - DiscNormalWorld * FVector::DotProduct(TorqueWorldNm, DiscNormalWorld);
        FVector NormalRate = TorquePerpendicular / SignedAngularMomentum;
        const float RateMagnitude = NormalRate.Size();
        if (RateMagnitude > MaxPrecessionRateRadPerSec)
        {
            NormalRate *= MaxPrecessionRateRadPerSec / RateMagnitude;
        }

        DiscNormalWorld = (DiscNormalWorld + NormalRate * Dt).GetSafeNormal();
    }

    SpinRateRadPerSec *= FMath::Exp(-Disc.Aero.SpinDecayPerSecond * Dt);

    FVector PreferredForward = VelocityMps - DiscNormalWorld * FVector::DotProduct(VelocityMps, DiscNormalWorld);
    if (PreferredForward.IsNearlyZero()) PreferredForward = DiscForwardWorld;
    RebuildBodyRotation(PreferredForward);
}

void UDiscFlightComponent::RebuildBodyRotation(const FVector& PreferredForward)
{
    FVector ForwardInPlane = PreferredForward - DiscNormalWorld * FVector::DotProduct(PreferredForward, DiscNormalWorld);
    ForwardInPlane = ForwardInPlane.GetSafeNormal();
    if (ForwardInPlane.IsNearlyZero()) ForwardInPlane = DiscForwardWorld;
    DiscForwardWorld = ForwardInPlane;
    BodyToWorld = FRotationMatrix::MakeFromXZ(DiscForwardWorld, DiscNormalWorld).ToQuat().GetNormalized();
}

void UDiscFlightComponent::SimulateFixedStep(float Dt)
{
    FString Error;
    if (!FMath::IsFinite(Dt) || Dt <= 0.0f
        || !ValidateSimulationConfiguration(Error))
    {
        if (Error.IsEmpty()) Error = TEXT("fixed-step delta is invalid");
        TerminateInvalidFlight(TEXT("fixed-step preflight"), Error);
        return;
    }
    if (!ValidateCurrentFlightState(Error))
    {
        TerminateInvalidFlight(TEXT("fixed-step state preflight"), Error);
        return;
    }
    if (State == EDiscFlightState::GroundPlay &&
        (GroundState == EDiscGroundState::Sliding || GroundState == EDiscGroundState::EdgeRolling))
    {
        SimulateGroundFixedStep(Dt);
        return;
    }

    SimulateAirborneFixedStep(Dt);
}

void UDiscFlightComponent::SimulateAirborneFixedStep(float Dt)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        StopFlight(false);
        return;
    }
    FString StateError;
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("airborne transform preflight"), StateError);
        return;
    }

    // Wind gust phase belongs to the fixed-step flight clock. Sampling the
    // WindDirector's render-ticked presentation clock here would make a group
    // of 1/240 s steps share one phase at 30 FPS but fewer steps at 120 FPS.
    const float WindSampleTimeSeconds =
        Telemetry.WindPhaseOriginSeconds + Telemetry.FlightTimeSeconds;
    FVector WindMps = FVector::ZeroVector;
    FString WindError;
    if (!TrySampleWindForFixedStep(
        WindDirector, Owner->GetActorLocation(), WindSampleTimeSeconds,
        WindMps, WindError))
    {
        TerminateInvalidFlight(TEXT("airborne wind sample"), WindError);
        return;
    }
    LastValidPhysicsWindMps = WindMps;
    const FVector AirVelocity = VelocityMps - WindMps;
    float AlphaRad = 0.0f;
    FVector UPlane = DiscForwardWorld;
    FVector ULateral = FVector::RightVector;
    const FVector AeroForceN = ComputeAeroForceN(AirVelocity, AlphaRad, UPlane, ULateral);
    const FVector GravityN(0.0f, 0.0f, -Disc.Aero.MassKg * 9.80665f);
    const FVector AccelerationMps2 = (AeroForceN + GravityN) / FMath::Max(Disc.Aero.MassKg, 1.0e-4f);
    const FVector TorqueWorld = ComputeAeroTorqueWorldNm(
        AirVelocity, AlphaRad, UPlane, ULateral);
    if (!IsFiniteVector(AirVelocity)
        || !IsFiniteVector(AeroForceN)
        || !IsFiniteVector(GravityN)
        || !IsFiniteVector(AccelerationMps2)
        || AccelerationMps2.Size() > MaximumRuntimeAccelerationMps2
        || !IsFiniteVector(TorqueWorld)
        || !FMath::IsFinite(AlphaRad))
    {
        TerminateInvalidFlight(
            TEXT("airborne force integration"), TEXT("derived aerodynamic state is non-finite"));
        return;
    }

    const FVector CandidateVelocityMps = VelocityMps + AccelerationMps2 * Dt;
    if (!IsFiniteVector(CandidateVelocityMps)
        || CandidateVelocityMps.Size() > MaximumRuntimeVelocityMps)
    {
        TerminateInvalidFlight(
            TEXT("airborne velocity integration"),
            TEXT("derived velocity exceeds the runtime envelope"));
        return;
    }
    VelocityMps = CandidateVelocityMps;
    const FVector DeltaCm = VelocityMps * (Dt * 100.0f);
    IntegrateAttitude(TorqueWorld, Dt);
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("airborne post-integration state"), StateError);
        return;
    }

    FHitResult Hit;
    const FVector NewLocation = Owner->GetActorLocation() + DeltaCm;
    if (!IsFiniteVector(NewLocation)
        || !IsFiniteVector(DeltaCm)
        || !IsFiniteQuaternion(BodyToWorld))
    {
        TerminateInvalidFlight(
            TEXT("airborne world transform"), TEXT("derived transform is non-finite"));
        return;
    }
    bool bMoveTransformValid = false;
    {
        FScopedMovementUpdate ScopedMovement(
            Owner->GetRootComponent(), EScopedUpdate::DeferredUpdates);
        Owner->SetActorLocationAndRotation(
            NewLocation, BodyToWorld, true, &Hit, ETeleportType::None);
        const FTransform AppliedOwnerTransform = Owner->GetActorTransform();
        bMoveTransformValid = IsOwnerTransformValidForLaunch(AppliedOwnerTransform)
            && AppliedOwnerTransform.GetRotation().Equals(
                BodyToWorld, KINDA_SMALL_NUMBER);
        if (bMoveTransformValid)
        {
            // Publish before deferred hit/overlap delegates execute at scope
            // destruction, so synchronous fixture callbacks observe the exact
            // authoritative transform that produced them.
            LastAuthoritativeOwnerTransform = AppliedOwnerTransform;
        }
        else
        {
            ScopedMovement.RevertMove();
        }
    }
    if (!bMoveTransformValid)
    {
        TerminateInvalidFlight(
            TEXT("airborne post-transform"),
            TEXT("actor transform or body-attitude coherence became invalid"));
        return;
    }
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("airborne deferred callbacks"), StateError);
        return;
    }
    if (!IsFlying()) return;
    if (Hit.bBlockingHit)
    {
        if (!IsContactNormalValid(Hit.ImpactNormal))
        {
            TerminateInvalidFlight(
                TEXT("airborne collision"), TEXT("blocking hit normal is invalid"));
            return;
        }
        ResolveHit(Hit);
        if (!IsFlying()) return;
    }
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("airborne post-hit state"), StateError);
        return;
    }

    Telemetry.FlightTimeSeconds += Dt;
    if (Telemetry.GroundContactCount > 0)
    {
        Telemetry.GroundPlayTimeSeconds += Dt;
    }
    UpdateTelemetry(AlphaRad);
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("airborne telemetry integration"), StateError);
        return;
    }
    RecordTrajectorySample();

    if (!DiscGolfMath::IsFlightStateWithinSafetyEnvelope(
        Owner->GetActorLocation(), VelocityMps, LaunchWorldLocation))
    {
        StopFlight(true);
        return;
    }

    if (Telemetry.FlightTimeSeconds >= DiscGolfMath::EffectiveFlightTimeoutSeconds(
        Telemetry.Release.ShotContext, Disc.Speed, MaxFlightSeconds))
    {
        StopFlight(true);
    }
}

void UDiscFlightComponent::SimulateGroundFixedStep(float Dt)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        StopFlight(false);
        return;
    }
    FString StateError;
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("ground transform preflight"), StateError);
        return;
    }

    const FVector Normal = GroundNormalWorld.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    FVector TangentialVelocity = VelocityMps - Normal * FVector::DotProduct(VelocityMps, Normal);
    const FVector GravityMps2(0.0f, 0.0f, -9.80665f);
    const FVector SlopeAcceleration = GravityMps2 - Normal * FVector::DotProduct(GravityMps2, Normal);
    TangentialVelocity += SlopeAcceleration * Dt;

    const float PlasticFrictionScale = FMath::Clamp(Disc.Aero.GroundFriction / 0.46f, 0.45f, 2.2f);
    const bool bEdgeRolling = GroundState == EDiscGroundState::EdgeRolling;
    const float DecelerationMps2 = (bEdgeRolling
        ? CurrentGroundProfile.RollDecelerationMps2
        : CurrentGroundProfile.SlideDecelerationMps2) * PlasticFrictionScale;
    TangentialVelocity = DiscGolfMath::ApplyGroundDeceleration(TangentialVelocity, DecelerationMps2, Dt);
    if (!IsFiniteVector(TangentialVelocity)
        || TangentialVelocity.Size() > MaximumRuntimeVelocityMps)
    {
        TerminateInvalidFlight(
            TEXT("ground velocity integration"),
            TEXT("derived velocity exceeds the runtime envelope"));
        return;
    }
    VelocityMps = TangentialVelocity;

    const float SpeedMps = TangentialVelocity.Size();
    if (bEdgeRolling && SpeedMps <= FMath::Max(CurrentGroundProfile.SettleSpeedMps * 1.45f, 1.35f))
    {
        const EDiscGroundState PreviousState = GroundState;
        GroundState = EDiscGroundState::Sliding;
        RecordGroundTransition(PreviousState, GroundState);
    }

    FVector TargetNormal = Normal;
    if (GroundState == EDiscGroundState::EdgeRolling && SpeedMps > KINDA_SMALL_NUMBER)
    {
        TargetNormal = FVector::CrossProduct(TangentialVelocity.GetSafeNormal(), Normal).GetSafeNormal();
        if (FVector::DotProduct(TargetNormal, DiscNormalWorld) < 0.0f) TargetNormal *= -1.0f;
    }
    const float AttitudeRate = GroundState == EDiscGroundState::EdgeRolling ? 7.0f : 4.0f;
    DiscNormalWorld = FMath::Lerp(DiscNormalWorld, TargetNormal, FMath::Clamp(AttitudeRate * Dt, 0.0f, 1.0f)).GetSafeNormal();
    SpinRateRadPerSec *= FMath::Exp(-CurrentGroundProfile.GroundSpinDecayPerSecond * PlasticFrictionScale * Dt);
    RebuildBodyRotation(TangentialVelocity.IsNearlyZero() ? DiscForwardWorld : TangentialVelocity);
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("ground post-integration state"), StateError);
        return;
    }

    FHitResult Hit;
    const FVector ContactVelocity = VelocityMps - Normal * GroundContactBiasMps;
    const FVector NewLocation = Owner->GetActorLocation() + ContactVelocity * (Dt * 100.0f);
    if (!IsFiniteVector(ContactVelocity)
        || !IsFiniteVector(NewLocation)
        || !IsFiniteQuaternion(BodyToWorld))
    {
        TerminateInvalidFlight(
            TEXT("ground world transform"), TEXT("derived transform is non-finite"));
        return;
    }
    bool bMoveTransformValid = false;
    {
        FScopedMovementUpdate ScopedMovement(
            Owner->GetRootComponent(), EScopedUpdate::DeferredUpdates);
        Owner->SetActorLocationAndRotation(
            NewLocation, BodyToWorld, true, &Hit, ETeleportType::None);
        const FTransform AppliedOwnerTransform = Owner->GetActorTransform();
        bMoveTransformValid = IsOwnerTransformValidForLaunch(AppliedOwnerTransform)
            && AppliedOwnerTransform.GetRotation().Equals(
                BodyToWorld, KINDA_SMALL_NUMBER);
        if (bMoveTransformValid)
        {
            LastAuthoritativeOwnerTransform = AppliedOwnerTransform;
        }
        else
        {
            ScopedMovement.RevertMove();
        }
    }
    if (!bMoveTransformValid)
    {
        TerminateInvalidFlight(
            TEXT("ground post-transform"),
            TEXT("actor transform or body-attitude coherence became invalid"));
        return;
    }
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("ground deferred callbacks"), StateError);
        return;
    }
    if (!IsFlying()) return;
    if (Hit.bBlockingHit)
    {
        if (!IsContactNormalValid(Hit.ImpactNormal))
        {
            TerminateInvalidFlight(
                TEXT("ground collision"), TEXT("blocking hit normal is invalid"));
            return;
        }
        if (Hit.ImpactNormal.Z > 0.40f)
        {
            if (!SetGroundContact(Hit))
            {
                TerminateInvalidFlight(
                    TEXT("ground contact"), TEXT("ground hit boundary is invalid"));
                return;
            }
            VelocityMps -= GroundNormalWorld * FMath::Min(FVector::DotProduct(VelocityMps, GroundNormalWorld), 0.0f);
        }
        else
        {
            ResolveObstacleHit(Hit);
        }
        if (!IsFlying()) return;
    }
    else
    {
        FHitResult SupportHit;
        if (ProbeGroundSupport(SupportHit))
        {
            if (!SetGroundContact(SupportHit))
            {
                TerminateInvalidFlight(
                    TEXT("ground support"), TEXT("support hit boundary is invalid"));
                return;
            }
        }
        else
        {
            TransitionGroundSupportLossToAirborne(Dt);
        }
    }
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("ground post-hit state"), StateError);
        return;
    }

    Telemetry.FlightTimeSeconds += Dt;
    Telemetry.GroundPlayTimeSeconds += Dt;
    Telemetry.GroundDistanceMeters += SpeedMps * Dt;
    UpdateTelemetry(0.0f);
    if (!ValidateCurrentFlightState(StateError))
    {
        TerminateInvalidFlight(TEXT("ground telemetry integration"), StateError);
        return;
    }
    RecordTrajectorySample();

    if (State == EDiscFlightState::GroundPlay &&
        (SpeedMps <= CurrentGroundProfile.SettleSpeedMps ||
         Telemetry.GroundPlayTimeSeconds >= MaxGroundPlaySeconds))
    {
        StopFlight(true);
        return;
    }

    if (Telemetry.FlightTimeSeconds >= DiscGolfMath::EffectiveFlightTimeoutSeconds(
        Telemetry.Release.ShotContext, Disc.Speed, MaxFlightSeconds))
    {
        StopFlight(true);
    }
}

void UDiscFlightComponent::TransitionGroundSupportLossToAirborne(float Dt)
{
    const EDiscGroundState PreviousState = GroundState;
    State = EDiscFlightState::Flying;
    GroundState = EDiscGroundState::Airborne;
    // The immediate post-hit validator compares solver and telemetry state.
    // Publish this atomic transition before that boundary; UpdateTelemetry
    // still owns the remaining derived values later in the fixed step.
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
    RecordGroundTransition(PreviousState, GroundState);
    VelocityMps += FVector(0.0f, 0.0f, -9.80665f) * Dt;
}

#if WITH_DEV_AUTOMATION_TESTS
bool UDiscFlightComponent::TriggerGroundSupportLossForTesting(
    float DeltaSeconds,
    FString& OutError)
{
    if (!IsFlying()
        || !FMath::IsFinite(DeltaSeconds)
        || DeltaSeconds <= 0.0f)
    {
        OutError = TEXT("test support-loss transition requires active flight and a positive finite delta");
        return false;
    }

    State = EDiscFlightState::GroundPlay;
    GroundState = EDiscGroundState::Sliding;
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
    TransitionGroundSupportLossToAirborne(DeltaSeconds);
    if (!ValidateCurrentFlightState(OutError))
    {
        TerminateInvalidFlight(TEXT("ground post-hit state"), OutError);
        return false;
    }
    OutError.Reset();
    return true;
}
#endif

void UDiscFlightComponent::ResolveHit(const FHitResult& Hit)
{
    if (!IsContactNormalValid(Hit.ImpactNormal)
        || !IsFiniteVector(Hit.ImpactPoint))
    {
        TerminateInvalidFlight(
            TEXT("collision dispatch"), TEXT("hit normal or impact point is invalid"));
        return;
    }
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();

    if (Cast<ADiscGolfWorldFixtureActor>(Hit.GetActor()))
    {
        ResolveObstacleHit(Hit);
        return;
    }

    if (Normal.Z > 0.40f)
    {
        ResolveGroundHit(Hit);
        return;
    }

    ResolveObstacleHit(Hit);
}

void UDiscFlightComponent::ResolveGroundHit(const FHitResult& Hit)
{
    if (!IsContactNormalValid(Hit.ImpactNormal)
        || !IsFiniteVector(Hit.ImpactPoint))
    {
        TerminateInvalidFlight(
            TEXT("ground impact"), TEXT("hit normal or impact point is invalid"));
        return;
    }
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    const ECourseSurfaceType ContactCourseSurface = ResolveCourseSurface(Hit);
    const EGroundSurfaceType ContactGroundSurface =
        DiscGolfCourseRules::GroundResponseSurface(ContactCourseSurface);
    const FGroundSurfaceProfile ContactGroundProfile =
        DiscGolfMath::GroundSurfaceProfile(ContactGroundSurface);
    if (!IsKnownCourseSurface(ContactCourseSurface)
        || !IsKnownGroundSurface(ContactGroundSurface)
        || !IsFiniteGroundProfile(ContactGroundProfile))
    {
        TerminateInvalidFlight(
            TEXT("ground impact surface"), TEXT("resolved surface profile is invalid"));
        return;
    }
    const float SpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
    const FGroundImpactResult Result = DiscGolfMath::ResolveGroundImpact(
        VelocityMps, Normal, DiscNormalWorld, SpinRpm,
        Disc.Aero.GroundRestitution, Disc.Aero.GroundFriction, ContactGroundSurface,
        ConsecutiveSkipCount);
    const float ResultScalars[] = {
        Result.SpinMultiplier,
        Result.ImpactSpeedMps,
        Result.ApproachSpeedMps,
        Result.TangentialSpeedMps,
        Result.IncidenceAngleDeg,
        Result.DiscEdgeAngleDeg,
        Result.EffectiveRestitution,
        Result.EffectiveFriction
    };
    bool bResultFinite = IsFiniteVector(Result.VelocityMps)
        && Result.VelocityMps.Size() <= MaximumRuntimeVelocityMps
        && IsKnownGroundState(Result.State)
        && Result.State != EDiscGroundState::Airborne
        && Result.State != EDiscGroundState::Impact
        && IsKnownGroundSurface(Result.Surface)
        && Result.Surface == ContactGroundSurface;
    for (const float Value : ResultScalars)
    {
        bResultFinite &= FMath::IsFinite(Value) && Value >= 0.0f;
    }
    const float ResultSpinRateRadPerSec = SpinRateRadPerSec * Result.SpinMultiplier;
    bResultFinite &= FMath::IsFinite(ResultSpinRateRadPerSec);
    if (!bResultFinite)
    {
        TerminateInvalidFlight(
            TEXT("ground impact resolution"), TEXT("resolved impact state is invalid"));
        return;
    }
    if (Telemetry.GroundContactCount >= MaximumRuntimeContactCount)
    {
        TerminateInvalidFlight(
            TEXT("ground contact saturation"),
            TEXT("ground contact count reached its runtime limit"));
        return;
    }

    const EDiscGroundState PreviousGroundState = GroundState;
    GroundNormalWorld = Normal;
    CourseSurface = ContactCourseSurface;
    GroundSurface = ContactGroundSurface;
    CurrentGroundProfile = ContactGroundProfile;
    Telemetry.GroundSurface = GroundSurface;
    Telemetry.CourseSurface = CourseSurface;
    GroundState = EDiscGroundState::Impact;
    ++Telemetry.GroundContactCount;
    Telemetry.LastImpactSpeedMps = Result.ImpactSpeedMps;
    Telemetry.LastImpactIncidenceDeg = Result.IncidenceAngleDeg;
    Telemetry.LastDiscEdgeAngleDeg = Result.DiscEdgeAngleDeg;
    VelocityMps = Result.VelocityMps;
    SpinRateRadPerSec = ResultSpinRateRadPerSec;
    GroundState = Result.State;
    ConsecutiveSkipCount = Result.State == EDiscGroundState::Skipping
        ? ConsecutiveSkipCount + 1
        : 0;
    RecordGroundTransition(
        PreviousGroundState,
        Result.State,
        Result.ImpactSpeedMps,
        Result.IncidenceAngleDeg,
        Result.DiscEdgeAngleDeg);

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Ground impact %d: %s on %s, speed %.2f m/s, incidence %.1f deg, edge %.1f deg, output %.2f m/s."),
        Telemetry.GroundContactCount, GroundStateName(Result.State), GroundSurfaceName(Result.Surface),
        Result.ImpactSpeedMps, Result.IncidenceAngleDeg, Result.DiscEdgeAngleDeg, Result.VelocityMps.Size());

    if (Result.State == EDiscGroundState::Settled)
    {
        StopFlight(true);
    }
    else if (Result.State == EDiscGroundState::Skipping)
    {
        State = EDiscFlightState::Flying;
    }
    else
    {
        State = EDiscFlightState::GroundPlay;
    }
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
}

void UDiscFlightComponent::ResolveObstacleHit(const FHitResult& Hit)
{
    if (!IsContactNormalValid(Hit.ImpactNormal)
        || !IsFiniteVector(Hit.ImpactPoint))
    {
        TerminateInvalidFlight(
            TEXT("obstacle impact"), TEXT("hit normal or impact point is invalid"));
        return;
    }
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();

    const ADiscGolfWorldFixtureActor* Fixture = Cast<ADiscGolfWorldFixtureActor>(Hit.GetActor());
    EDiscGolfFixtureType FixtureType = Fixture
        ? Fixture->FixtureType : EDiscGolfFixtureType::Unknown;
    if (!Fixture && Hit.GetComponent())
    {
        const TArray<FName>& HitComponentTags = Hit.GetComponent()->ComponentTags;
        if (HitComponentTags.Contains(TEXT("Environment.Collision.Tree")))
        {
            FixtureType = EDiscGolfFixtureType::Tree;
        }
        else if (HitComponentTags.Contains(TEXT("Environment.Collision.Rock")))
        {
            FixtureType = EDiscGolfFixtureType::Rock;
        }
    }
    const FVector EntryVelocityMps = VelocityMps;
    const float EntrySpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
    const FDiscGolfFixtureImpactResult Result = DiscGolfMath::ResolveFixtureImpact(
        EntryVelocityMps, Normal, FixtureType);
    const float ResultSpinRateRadPerSec = SpinRateRadPerSec * Result.SpinMultiplier;
    if (!IsFiniteVector(Result.VelocityMps)
        || Result.VelocityMps.Size() > MaximumRuntimeVelocityMps
        || !FMath::IsFinite(Result.SpinMultiplier)
        || Result.SpinMultiplier < 0.0f
        || !FMath::IsFinite(Result.ImpactSpeedMps)
        || Result.ImpactSpeedMps < 0.0f
        || !FMath::IsFinite(Result.NormalSpeedMps)
        || Result.NormalSpeedMps < 0.0f
        || !FMath::IsFinite(ResultSpinRateRadPerSec))
    {
        TerminateInvalidFlight(
            TEXT("obstacle impact resolution"), TEXT("resolved fixture state is invalid"));
        return;
    }
    const bool bRecordsFixtureContact = Fixture
        || FixtureType != EDiscGolfFixtureType::Unknown;
    if (bRecordsFixtureContact
        && Telemetry.FixtureContactCount >= MaximumRuntimeContactCount)
    {
        TerminateInvalidFlight(
            TEXT("fixture impact saturation"),
            TEXT("fixture contact count reached its runtime limit"));
        return;
    }
    VelocityMps = Result.VelocityMps;
    SpinRateRadPerSec = ResultSpinRateRadPerSec;
    Telemetry.LastImpactSpeedMps = Result.ImpactSpeedMps;
    if (bRecordsFixtureContact)
    {
        ++Telemetry.FixtureContactCount;
        Telemetry.LastFixtureType = Result.FixtureType;
        Telemetry.LastFixtureImpactSpeedMps = Result.ImpactSpeedMps;
        Telemetry.LastFixtureEntryVelocityMps = EntryVelocityMps;
        Telemetry.LastFixtureExitVelocityMps = VelocityMps;
        Telemetry.LastFixtureImpactNormal = Normal;
        Telemetry.LastFixtureEntrySpinRpm = EntrySpinRpm;
        Telemetry.LastFixtureExitSpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("Fixture impact %s: speed %.2f -> %.2f m/s, normal %.2f m/s, spin retention %.2f."),
            Fixture ? *Fixture->FixtureId.ToString() : TEXT("Environment.DecorativeVegetation"),
            Result.ImpactSpeedMps, Result.VelocityMps.Size(),
            Result.NormalSpeedMps, Result.SpinMultiplier);
    }

    if (State == EDiscFlightState::GroundPlay && VelocityMps.Size() < CurrentGroundProfile.SettleSpeedMps)
    {
        StopFlight(true);
    }
}

bool UDiscFlightComponent::ProbeGroundSupport(FHitResult& OutHit) const
{
    const AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World) return false;

    const FVector Normal = GroundNormalWorld.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    const FVector Start = Owner->GetActorLocation() + Normal * 8.0f;
    const FVector End = Owner->GetActorLocation() - Normal * GroundProbeDepthCm;
    if (!IsContactNormalValid(Normal)
        || !IsFiniteVector(Start)
        || !IsFiniteVector(End))
    {
        return false;
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DiscGroundSupport), false, Owner);
    Params.bReturnPhysicalMaterial = true;
    const bool bHit = World->LineTraceSingleByChannel(
        OutHit, Start, End, ECC_Visibility, Params);
    return bHit
        && IsContactNormalValid(OutHit.ImpactNormal)
        && IsFiniteVector(OutHit.ImpactPoint)
        && OutHit.ImpactNormal.Z > 0.40f;
}

ECourseSurfaceType UDiscFlightComponent::ResolveCourseSurface(const FHitResult& Hit) const
{
    const AActor* HitActor = Hit.GetActor();
    if (HitActor && HitActor->ActorHasTag(TEXT("Presentation.CourseTerrain")))
    {
        if (const UWorld* World = GetWorld())
        {
            if (const ADiscGolfTourGameMode* GameMode =
                World->GetAuthGameMode<ADiscGolfTourGameMode>())
            {
                ECourseSurfaceType Surface = ECourseSurfaceType::Fairway;
                FVector GroundLocation = Hit.ImpactPoint;
                if (GameMode->TraceCourseSurfaceAtLocation(
                    Hit.ImpactPoint, Surface, GroundLocation))
                {
                    return Surface;
                }
            }
        }
    }
    return DiscGolfCourseRules::ResolveSurface(Hit.GetActor(), Hit.PhysMaterial.Get());
}

bool UDiscFlightComponent::SetGroundContact(const FHitResult& Hit)
{
    if (!IsContactNormalValid(Hit.ImpactNormal)
        || !IsFiniteVector(Hit.ImpactPoint))
    {
        return false;
    }

    const FVector ResolvedGroundNormal = Hit.ImpactNormal.GetSafeNormal();
    const ECourseSurfaceType ResolvedCourseSurface = ResolveCourseSurface(Hit);
    if (!IsKnownCourseSurface(ResolvedCourseSurface))
    {
        return false;
    }
    const EGroundSurfaceType ResolvedGroundSurface =
        DiscGolfCourseRules::GroundResponseSurface(ResolvedCourseSurface);
    const FGroundSurfaceProfile ResolvedGroundProfile =
        DiscGolfMath::GroundSurfaceProfile(ResolvedGroundSurface);
    if (!IsKnownGroundSurface(ResolvedGroundSurface)
        || !IsFiniteGroundProfile(ResolvedGroundProfile))
    {
        return false;
    }

    GroundNormalWorld = ResolvedGroundNormal;
    CourseSurface = ResolvedCourseSurface;
    GroundSurface = ResolvedGroundSurface;
    CurrentGroundProfile = ResolvedGroundProfile;
    Telemetry.GroundSurface = GroundSurface;
    Telemetry.CourseSurface = CourseSurface;
    return true;
}

void UDiscFlightComponent::UpdateTelemetry(float AlphaRad)
{
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
    Telemetry.GroundSurface = GroundSurface;
    Telemetry.CourseSurface = CourseSurface;
    Telemetry.VelocityMps = VelocityMps;
    Telemetry.SpeedMps = VelocityMps.Size();
    Telemetry.SpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
    Telemetry.AngleOfAttackDeg = FMath::RadiansToDegrees(AlphaRad);
    Telemetry.CarryMeters = FVector::Dist2D(LaunchWorldLocation, GetOwner()->GetActorLocation()) / 100.0f;
}
void UDiscFlightComponent::RecordTrajectorySample(bool bForce)
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    FString Error;
    if (!ValidateSimulationConfiguration(Error)
        || !ValidateCurrentFlightState(Error))
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Trajectory sample rejected at the solver boundary: %s"), *Error);
        return;
    }

    const float SampleInterval = 1.0f / TrajectorySampleHz;
    if (!bForce && Telemetry.FlightTimeSeconds + KINDA_SMALL_NUMBER < NextTrajectorySampleTime) return;

    FDiscTrajectorySample Sample;
    Sample.TimeSeconds = Telemetry.FlightTimeSeconds;
    Sample.WorldLocationCm = Owner->GetActorLocation();
    Sample.VelocityMps = VelocityMps;
    Sample.DiscNormalWorld = DiscNormalWorld;
    Sample.WindMps = LastValidPhysicsWindMps;
    const float WindSampleTimeSeconds =
        Telemetry.WindPhaseOriginSeconds + Telemetry.FlightTimeSeconds;
    FVector SampledWindMps = FVector::ZeroVector;
    FString WindError;
    if (TrySampleWindForFixedStep(
        WindDirector, Owner->GetActorLocation(), WindSampleTimeSeconds,
        SampledWindMps, WindError))
    {
        LastValidPhysicsWindMps = SampledWindMps;
        Sample.WindMps = SampledWindMps;
    }
    Sample.SpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
    Sample.AngleOfAttackDeg = Telemetry.AngleOfAttackDeg;
    Sample.GroundState = GroundState;
    Sample.GroundSurface = GroundSurface;
    Sample.CourseSurface = CourseSurface;
    Sample.GroundContactCount = Telemetry.GroundContactCount;

    // Forced state transitions can occur without advancing solver time (for
    // example Sliding -> Settled). Keep one canonical actual state for a given
    // timestamp so replay never has to choose between two positions/states at
    // zero duration. GroundTransitions retains the discrete transition itself.
    if (!TrajectorySamples.IsEmpty()
        && FMath::IsNearlyEqual(
            TrajectorySamples.Last().TimeSeconds,
            Sample.TimeSeconds,
            KINDA_SMALL_NUMBER))
    {
        TrajectorySamples.Last() = Sample;
    }
    else
    {
        TrajectorySamples.Add(Sample);
    }
    if (!bTrajectorySampleScheduleInitialized)
    {
        NextTrajectorySampleTime = Telemetry.FlightTimeSeconds + SampleInterval;
        bTrajectorySampleScheduleInitialized = true;
    }
    else if (!bForce)
    {
        do
        {
            NextTrajectorySampleTime += SampleInterval;
        }
        while (NextTrajectorySampleTime <= Telemetry.FlightTimeSeconds);
    }
}

void UDiscFlightComponent::RecordGroundTransition(
    EDiscGroundState FromState,
    EDiscGroundState ToState,
    float ImpactSpeedMps,
    float IncidenceAngleDeg,
    float DiscEdgeAngleDeg)
{
    if ((FromState == ToState && ToState != EDiscGroundState::Skipping)
        || !IsKnownGroundState(FromState)
        || !IsKnownGroundState(ToState)
        || !FMath::IsFinite(ImpactSpeedMps)
        || !FMath::IsFinite(IncidenceAngleDeg)
        || !FMath::IsFinite(DiscEdgeAngleDeg))
    {
        return;
    }
    const AActor* Owner = GetOwner();
    if (!Owner || !IsOwnerTransformValidForLaunch(Owner->GetActorTransform()))
    {
        return;
    }

    FDiscGroundTransition Transition;
    Transition.TimeSeconds = Telemetry.FlightTimeSeconds;
    Transition.WorldLocationCm = Owner->GetActorLocation();
    Transition.FromState = FromState;
    Transition.ToState = ToState;
    Transition.Surface = GroundSurface;
    Transition.CourseSurface = CourseSurface;
    Transition.GroundContactCount = Telemetry.GroundContactCount;
    Transition.ImpactSpeedMps = ImpactSpeedMps;
    Transition.IncidenceAngleDeg = IncidenceAngleDeg;
    Transition.DiscEdgeAngleDeg = DiscEdgeAngleDeg;
    GroundTransitions.Add(Transition);
}
