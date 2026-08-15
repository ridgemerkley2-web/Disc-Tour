#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfTypes.h"
#include "DiscFlightComponent.generated.h"

class AWindDirector;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiscFlightSettledSignature, FDiscFlightTelemetry, Telemetry);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UDiscFlightComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscFlightComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(BlueprintAssignable) FDiscFlightSettledSignature OnFlightSettled;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float AirDensityKgM3 = 1.225f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float FixedStepSeconds = 1.0f / 240.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float MaxFlightSeconds = 30.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float MaxGroundPlaySeconds = 12.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float MaxPrecessionRateRadPerSec = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float GroundContactBiasMps = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float GroundProbeDepthCm = 38.0f;
    /** 240 Hz by default so each solver step is retained for export/replay consumers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry") float TrajectorySampleHz = 240.0f;

    UFUNCTION(BlueprintCallable) void ConfigureDisc(const FResolvedDiscDefinition& InDisc);
    UFUNCTION(BlueprintCallable) void SetWindDirector(AWindDirector* InWindDirector);
    UFUNCTION(BlueprintCallable) void Launch(const FThrowRelease& Release);
    void ApplyBasketContact(const FBasketContactEvaluation& Evaluation, const FVector& CaptureWorldLocationCm);
    void ApplyFixtureOverlap(AActor* FixtureActor);
    void StopFlight(bool bNotifySettled = true);

    UFUNCTION(BlueprintPure) bool IsFlying() const { return State == EDiscFlightState::Flying || State == EDiscFlightState::GroundPlay; }
    UFUNCTION(BlueprintPure) FVector GetVelocityMps() const { return VelocityMps; }
    UFUNCTION(BlueprintPure) FDiscFlightTelemetry GetTelemetry() const { return Telemetry; }
    UFUNCTION(BlueprintPure) FResolvedDiscDefinition GetDisc() const { return Disc; }
    const TArray<FDiscTrajectorySample>& GetTrajectorySamples() const { return TrajectorySamples; }
    const TArray<FDiscGroundTransition>& GetGroundTransitions() const { return GroundTransitions; }

private:
    UPROPERTY() TObjectPtr<AWindDirector> WindDirector;
    FResolvedDiscDefinition Disc;
    FDiscFlightTelemetry Telemetry;
    EDiscFlightState State = EDiscFlightState::Idle;
    EDiscGroundState GroundState = EDiscGroundState::Airborne;
    EGroundSurfaceType GroundSurface = EGroundSurfaceType::Fairway;
    ECourseSurfaceType CourseSurface = ECourseSurfaceType::Fairway;
    FGroundSurfaceProfile CurrentGroundProfile;
    FVector VelocityMps = FVector::ZeroVector;
    FVector DiscNormalWorld = FVector::UpVector;
    FVector DiscForwardWorld = FVector::ForwardVector;
    FVector GroundNormalWorld = FVector::UpVector;
    float SpinRateRadPerSec = 0.0f;
    FQuat BodyToWorld = FQuat::Identity;
    FVector LaunchWorldLocation = FVector::ZeroVector;
    double Accumulator = 0.0;
    int32 ConsecutiveSkipCount = 0;
    TArray<FDiscTrajectorySample> TrajectorySamples;
    TArray<FDiscGroundTransition> GroundTransitions;
    TMap<TWeakObjectPtr<AActor>, float> VegetationOverlapTimes;
    float NextTrajectorySampleTime = 0.0f;

    void SimulateFixedStep(float Dt);
    void SimulateAirborneFixedStep(float Dt);
    void SimulateGroundFixedStep(float Dt);
    FVector ComputeAeroForceN(const FVector& AirVelocityMps, float& OutAlphaRad, FVector& OutUPlane, FVector& OutULateral) const;
    FVector ComputeAeroTorqueWorldNm(const FVector& AirVelocityMps, float AlphaRad, const FVector& UPlane, const FVector& ULateral) const;
    void IntegrateAttitude(const FVector& TorqueWorldNm, float Dt);
    void ResolveHit(const FHitResult& Hit);
    void ResolveGroundHit(const FHitResult& Hit);
    void ResolveObstacleHit(const FHitResult& Hit);
    bool ProbeGroundSupport(FHitResult& OutHit) const;
    ECourseSurfaceType ResolveCourseSurface(const FHitResult& Hit) const;
    void SetGroundContact(const FHitResult& Hit);
    void UpdateTelemetry(float AlphaRad);
    void RebuildBodyRotation(const FVector& PreferredForward);
    void RecordTrajectorySample(bool bForce = false);
    void RecordGroundTransition(
        EDiscGroundState FromState,
        EDiscGroundState ToState,
        float ImpactSpeedMps = 0.0f,
        float IncidenceAngleDeg = 0.0f,
        float DiscEdgeAngleDeg = 0.0f);
};
