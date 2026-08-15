#include "DiscFlightComponent.h"
#include "DiscGolfCourseRules.h"
#include "DiscGolfTour.h"
#include "WindDirector.h"
#include "DiscGolfMath.h"
#include "DiscGolfWorldFixtureActor.h"
#include "DiscGolfVegetationInteractionActor.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
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

void UDiscFlightComponent::ConfigureDisc(const FResolvedDiscDefinition& InDisc)
{
    Disc = InDisc;
}

void UDiscFlightComponent::SetWindDirector(AWindDirector* InWindDirector)
{
    WindDirector = InWindDirector;
}

void UDiscFlightComponent::Launch(const FThrowRelease& Release)
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    FVector FlatDirection(Release.Direction.X, Release.Direction.Y, 0.0f);
    FlatDirection = FlatDirection.GetSafeNormal();
    if (FlatDirection.IsNearlyZero())
    {
        FlatDirection = Owner->GetActorForwardVector().GetSafeNormal2D();
    }

    const FVector WorldUp = FVector::UpVector;
    FVector Right = FVector::CrossProduct(WorldUp, FlatDirection).GetSafeNormal();
    if (Right.IsNearlyZero()) Right = FVector::RightVector;

    const FQuat AimError(WorldUp, FMath::DegreesToRadians(Release.AimOffsetDeg));
    FlatDirection = AimError.RotateVector(FlatDirection).GetSafeNormal();
    Right = FVector::CrossProduct(WorldUp, FlatDirection).GetSafeNormal();

    const FVector LaunchDirection = DiscGolfMath::LaunchDirectionFromFlat(
        FlatDirection, Release.EffectiveLaunchAngleDeg);
    VelocityMps = LaunchDirection * Release.ReleaseSpeedMps;

    const float HandSign = Release.ThrowStyle == EThrowStyle::Backhand ? 1.0f : -1.0f;

    // Nose angle is defined relative to the launch trajectory, not the horizon.
    // Positive nose means the leading edge is above the trajectory.
    const FQuat NoseRotation(Right, FMath::DegreesToRadians(-Release.EffectiveNoseAngleDeg));
    DiscForwardWorld = NoseRotation.RotateVector(LaunchDirection).GetSafeNormal();
    DiscNormalWorld = FVector::CrossProduct(DiscForwardWorld, Right).GetSafeNormal();

    // Positive hyzer means the outside edge is down. Forehand mirrors the bank direction.
    const FQuat HyzerRotation(DiscForwardWorld, FMath::DegreesToRadians(HandSign * Release.EffectiveHyzerDeg));
    DiscNormalWorld = HyzerRotation.RotateVector(DiscNormalWorld).GetSafeNormal();
    RebuildBodyRotation(DiscForwardWorld);
    Owner->SetActorRotation(BodyToWorld);

    SpinRateRadPerSec = HandSign * Release.SpinRpm * 2.0f * PI / 60.0f;

    LaunchWorldLocation = Owner->GetActorLocation();
    Accumulator = 0.0;
    State = EDiscFlightState::Flying;
    GroundState = EDiscGroundState::Airborne;
    GroundSurface = EGroundSurfaceType::Fairway;
    CourseSurface = ECourseSurfaceType::Fairway;
    CurrentGroundProfile = DiscGolfMath::GroundSurfaceProfile(GroundSurface);
    GroundNormalWorld = FVector::UpVector;
    ConsecutiveSkipCount = 0;
    Telemetry = FDiscFlightTelemetry();
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
    Telemetry.GroundSurface = GroundSurface;
    Telemetry.CourseSurface = CourseSurface;
    Telemetry.Release = Release;
    TrajectorySamples.Reset();
    GroundTransitions.Reset();
    VegetationOverlapTimes.Reset();
    NextTrajectorySampleTime = 0.0f;
    UpdateTelemetry(0.0f);
    RecordTrajectorySample(true);
}

void UDiscFlightComponent::StopFlight(bool bNotifySettled)
{
    const bool bWasActive = IsFlying();
    const EDiscGroundState PreviousGroundState = GroundState;
    VelocityMps = FVector::ZeroVector;
    SpinRateRadPerSec = 0.0f;
    Accumulator = 0.0;
    State = EDiscFlightState::Settled;
    GroundState = EDiscGroundState::Settled;
    Telemetry.State = State;
    Telemetry.GroundState = GroundState;
    if (bWasActive && Telemetry.GroundContactCount > 0 && PreviousGroundState != EDiscGroundState::Settled)
    {
        RecordGroundTransition(PreviousGroundState, EDiscGroundState::Settled);
    }
    RecordTrajectorySample(true);

    if (bWasActive && Telemetry.GroundContactCount > 0)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("Ground settled on %s after %d impact(s), %.2f m ground travel, %.2f s after first contact."),
            GroundSurfaceName(GroundSurface), Telemetry.GroundContactCount,
            Telemetry.GroundDistanceMeters, Telemetry.GroundPlayTimeSeconds);
    }

    if (bNotifySettled && bWasActive)
    {
        OnFlightSettled.Broadcast(Telemetry);
    }
}

void UDiscFlightComponent::ApplyBasketContact(
    const FBasketContactEvaluation& Evaluation,
    const FVector& CaptureWorldLocationCm)
{
    if (!IsFlying() || Evaluation.Result == EBasketContactResult::None) return;

    ++Telemetry.BasketContactCount;
    Telemetry.LastBasketContact = Evaluation.Result;
    if (Evaluation.Result == EBasketContactResult::Caught)
    {
        if (AActor* Owner = GetOwner())
        {
            Owner->SetActorLocation(CaptureWorldLocationCm, false, nullptr, ETeleportType::TeleportPhysics);
        }
        VelocityMps = FVector::ZeroVector;
        UpdateTelemetry(0.0f);
        RecordTrajectorySample(true);
        return;
    }

    VelocityMps = Evaluation.DeflectedVelocityMps;
    SpinRateRadPerSec *= Evaluation.Result == EBasketContactResult::ChainDeflection ? 0.42f : 0.68f;
    UpdateTelemetry(0.0f);
    RecordTrajectorySample(true);
}

void UDiscFlightComponent::ApplyFixtureOverlap(AActor* FixtureActor)
{
    if (!IsFlying()) return;
    if (const ADiscGolfVegetationInteractionActor* Vegetation =
        Cast<ADiscGolfVegetationInteractionActor>(FixtureActor))
    {
        if (!Vegetation->HasValidInteractionContract()) return;
        const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        const TWeakObjectPtr<AActor> Key(FixtureActor);
        if (const float* Previous = VegetationOverlapTimes.Find(Key))
        {
            if (Now - *Previous < Vegetation->Profile.ReentryCooldownSeconds) return;
        }
        VegetationOverlapTimes.Add(Key, Now);
        VelocityMps *= FMath::Clamp(Vegetation->Profile.VelocityMultiplier, 0.05f, 1.0f);
        SpinRateRadPerSec *= FMath::Clamp(Vegetation->Profile.SpinMultiplier, 0.05f, 1.0f);
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
    VelocityMps = Result.VelocityMps;
    SpinRateRadPerSec *= Result.SpinMultiplier;
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
    if (!IsFlying() || FixedStepSeconds <= 0.0f) return;

    Accumulator += FMath::Min<double>(DeltaTime, 0.1);
    int32 Steps = 0;
    constexpr int32 MaxStepsPerFrame = 32;
    while (Accumulator >= FixedStepSeconds && Steps++ < MaxStepsPerFrame && IsFlying())
    {
        SimulateFixedStep(FixedStepSeconds);
        Accumulator -= FixedStepSeconds;
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

    const FVector WindMps = WindDirector ? WindDirector->GetWindMpsAt(Owner->GetActorLocation()) : FVector::ZeroVector;
    const FVector AirVelocity = VelocityMps - WindMps;
    float AlphaRad = 0.0f;
    FVector UPlane = DiscForwardWorld;
    FVector ULateral = FVector::RightVector;
    const FVector AeroForceN = ComputeAeroForceN(AirVelocity, AlphaRad, UPlane, ULateral);
    const FVector GravityN(0.0f, 0.0f, -Disc.Aero.MassKg * 9.80665f);
    const FVector AccelerationMps2 = (AeroForceN + GravityN) / FMath::Max(Disc.Aero.MassKg, 1.0e-4f);

    VelocityMps += AccelerationMps2 * Dt;
    const FVector DeltaCm = VelocityMps * (Dt * 100.0f);

    const FVector TorqueWorld = ComputeAeroTorqueWorldNm(AirVelocity, AlphaRad, UPlane, ULateral);
    IntegrateAttitude(TorqueWorld, Dt);

    FHitResult Hit;
    const FVector NewLocation = Owner->GetActorLocation() + DeltaCm;
    Owner->SetActorLocationAndRotation(NewLocation, BodyToWorld, true, &Hit, ETeleportType::None);
    if (Hit.bBlockingHit)
    {
        ResolveHit(Hit);
    }

    Telemetry.FlightTimeSeconds += Dt;
    if (Telemetry.GroundContactCount > 0)
    {
        Telemetry.GroundPlayTimeSeconds += Dt;
    }
    UpdateTelemetry(AlphaRad);
    RecordTrajectorySample();

    if (Telemetry.FlightTimeSeconds >= MaxFlightSeconds)
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

    FHitResult Hit;
    const FVector ContactVelocity = VelocityMps - Normal * GroundContactBiasMps;
    const FVector NewLocation = Owner->GetActorLocation() + ContactVelocity * (Dt * 100.0f);
    Owner->SetActorLocationAndRotation(NewLocation, BodyToWorld, true, &Hit, ETeleportType::None);
    if (Hit.bBlockingHit)
    {
        if (Hit.ImpactNormal.Z > 0.40f)
        {
            SetGroundContact(Hit);
            VelocityMps -= GroundNormalWorld * FMath::Min(FVector::DotProduct(VelocityMps, GroundNormalWorld), 0.0f);
        }
        else
        {
            ResolveObstacleHit(Hit);
        }
    }
    else
    {
        FHitResult SupportHit;
        if (ProbeGroundSupport(SupportHit))
        {
            SetGroundContact(SupportHit);
        }
        else
        {
            const EDiscGroundState PreviousState = GroundState;
            State = EDiscFlightState::Flying;
            GroundState = EDiscGroundState::Airborne;
            RecordGroundTransition(PreviousState, GroundState);
            VelocityMps += GravityMps2 * Dt;
        }
    }

    Telemetry.FlightTimeSeconds += Dt;
    Telemetry.GroundPlayTimeSeconds += Dt;
    Telemetry.GroundDistanceMeters += SpeedMps * Dt;
    UpdateTelemetry(0.0f);
    RecordTrajectorySample();

    if (State == EDiscFlightState::GroundPlay &&
        (SpeedMps <= CurrentGroundProfile.SettleSpeedMps ||
         Telemetry.GroundPlayTimeSeconds >= MaxGroundPlaySeconds))
    {
        StopFlight(true);
        return;
    }

    if (Telemetry.FlightTimeSeconds >= MaxFlightSeconds)
    {
        StopFlight(true);
    }
}

void UDiscFlightComponent::ResolveHit(const FHitResult& Hit)
{
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    if (Normal.IsNearlyZero())
    {
        StopFlight(true);
        return;
    }

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
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    const EDiscGroundState PreviousGroundState = GroundState;
    GroundState = EDiscGroundState::Impact;
    SetGroundContact(Hit);

    const float SpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
    const FGroundImpactResult Result = DiscGolfMath::ResolveGroundImpact(
        VelocityMps, Normal, DiscNormalWorld, SpinRpm,
        Disc.Aero.GroundRestitution, Disc.Aero.GroundFriction, GroundSurface,
        ConsecutiveSkipCount);

    ++Telemetry.GroundContactCount;
    Telemetry.LastImpactSpeedMps = Result.ImpactSpeedMps;
    Telemetry.LastImpactIncidenceDeg = Result.IncidenceAngleDeg;
    Telemetry.LastDiscEdgeAngleDeg = Result.DiscEdgeAngleDeg;
    VelocityMps = Result.VelocityMps;
    SpinRateRadPerSec *= Result.SpinMultiplier;
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
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    if (Normal.IsNearlyZero())
    {
        StopFlight(true);
        return;
    }

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
    VelocityMps = Result.VelocityMps;
    SpinRateRadPerSec *= Result.SpinMultiplier;
    Telemetry.LastImpactSpeedMps = Result.ImpactSpeedMps;
    if (Fixture || FixtureType != EDiscGolfFixtureType::Unknown)
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
            Fixture ? *Fixture->FixtureId.ToString() : TEXT("PCG.Environment"),
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
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DiscGroundSupport), false, Owner);
    Params.bReturnPhysicalMaterial = true;
    return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params) && OutHit.ImpactNormal.Z > 0.40f;
}

ECourseSurfaceType UDiscFlightComponent::ResolveCourseSurface(const FHitResult& Hit) const
{
    return DiscGolfCourseRules::ResolveSurface(Hit.GetActor(), Hit.PhysMaterial.Get());
}

void UDiscFlightComponent::SetGroundContact(const FHitResult& Hit)
{
    GroundNormalWorld = Hit.ImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    CourseSurface = ResolveCourseSurface(Hit);
    GroundSurface = DiscGolfCourseRules::GroundResponseSurface(CourseSurface);
    CurrentGroundProfile = DiscGolfMath::GroundSurfaceProfile(GroundSurface);
    Telemetry.GroundSurface = GroundSurface;
    Telemetry.CourseSurface = CourseSurface;
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

    const float SampleInterval = 1.0f / FMath::Max(TrajectorySampleHz, 1.0f);
    if (!bForce && Telemetry.FlightTimeSeconds + KINDA_SMALL_NUMBER < NextTrajectorySampleTime) return;

    FDiscTrajectorySample Sample;
    Sample.TimeSeconds = Telemetry.FlightTimeSeconds;
    Sample.WorldLocationCm = Owner->GetActorLocation();
    Sample.VelocityMps = VelocityMps;
    Sample.DiscNormalWorld = DiscNormalWorld;
    Sample.WindMps = WindDirector ? WindDirector->GetWindMpsAt(Owner->GetActorLocation()) : FVector::ZeroVector;
    Sample.SpinRpm = FMath::Abs(SpinRateRadPerSec) * 60.0f / (2.0f * PI);
    Sample.AngleOfAttackDeg = Telemetry.AngleOfAttackDeg;
    Sample.GroundState = GroundState;
    Sample.GroundSurface = GroundSurface;
    Sample.CourseSurface = CourseSurface;
    Sample.GroundContactCount = Telemetry.GroundContactCount;
    TrajectorySamples.Add(Sample);
    NextTrajectorySampleTime = Telemetry.FlightTimeSeconds + SampleInterval;
}

void UDiscFlightComponent::RecordGroundTransition(
    EDiscGroundState FromState,
    EDiscGroundState ToState,
    float ImpactSpeedMps,
    float IncidenceAngleDeg,
    float DiscEdgeAngleDeg)
{
    if (FromState == ToState && ToState != EDiscGroundState::Skipping) return;

    FDiscGroundTransition Transition;
    Transition.TimeSeconds = Telemetry.FlightTimeSeconds;
    Transition.WorldLocationCm = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
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
