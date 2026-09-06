#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfTypes.h"
#include "WindDirector.h"
#include "DiscFlightComponent.generated.h"

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation", meta=(ClampMin="0.01", ClampMax="5.0"))
    float AirDensityKgM3 = 1.225f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation", meta=(ClampMin="0.001", ClampMax="0.0333333"))
    float FixedStepSeconds = 1.0f / 240.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation", meta=(ClampMin="0.1", ClampMax="120.0"))
    float MaxFlightSeconds = 30.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation", meta=(ClampMin="0.1", ClampMax="60.0"))
    float MaxGroundPlaySeconds = 12.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation", meta=(ClampMin="0.01", ClampMax="20.0"))
    float MaxPrecessionRateRadPerSec = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation", meta=(ClampMin="0.0", ClampMax="5.0"))
    float GroundContactBiasMps = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation", meta=(ClampMin="1.0", ClampMax="1000.0"))
    float GroundProbeDepthCm = 38.0f;
    /** 240 Hz by default so each solver step is retained for export/replay consumers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="1.0", ClampMax="1000.0"))
    float TrajectorySampleHz = 240.0f;

    UFUNCTION(BlueprintCallable) bool ConfigureDisc(const FResolvedDiscDefinition& InDisc);
    UFUNCTION(BlueprintCallable) bool SetWindDirector(AWindDirector* InWindDirector);
    UFUNCTION(BlueprintCallable) bool Launch(const FThrowRelease& Release);
    bool ApplyBasketContact(const FBasketContactEvaluation& Evaluation, const FVector& CaptureWorldLocationCm);
    void ApplyFixtureOverlap(AActor* FixtureActor);
    void StopFlight(bool bNotifySettled = true);

    /** Complete finite/range contract for all designer-editable solver settings. */
    bool ValidateSimulationConfiguration(FString& OutError) const;

    /** Pure boundary seams used by pre-spawn, launch/hit handling, and focused automation. */
    static bool IsSpawnTransformValidForLaunch(const FTransform& Transform);
    static bool IsOwnerTransformValidForLaunch(const FTransform& Transform);
    static bool IsContactNormalValid(const FVector& Normal);

    /** C++ seam shared by fixed-step integration, telemetry, and automation. */
    static FVector SampleWindForFixedStep(
        const AWindDirector* InWindDirector,
        const FVector& WorldLocation,
        float SimulationTimeSeconds);
    static bool TrySampleWindForFixedStep(
        const AWindDirector* InWindDirector,
        const FVector& WorldLocation,
        float SimulationTimeSeconds,
        FVector& OutWindMps,
        FString& OutError);

#if WITH_DEV_AUTOMATION_TESTS
    /** Exercises the production ground-support-loss transition without a scene trace. */
    bool TriggerGroundSupportLossForTesting(float DeltaSeconds, FString& OutError);
#endif

    UFUNCTION(BlueprintPure) bool IsFlying() const { return State == EDiscFlightState::Flying || State == EDiscFlightState::GroundPlay; }
    UFUNCTION(BlueprintPure) bool IsDiscConfigured() const { return bDiscConfigured; }
    UFUNCTION(BlueprintPure) EDiscFlightState GetFlightState() const { return State; }
    UFUNCTION(BlueprintPure) FVector GetVelocityMps() const { return VelocityMps; }
    UFUNCTION(BlueprintPure) FDiscFlightTelemetry GetTelemetry() const { return Telemetry; }
    UFUNCTION(BlueprintPure) FResolvedDiscDefinition GetDisc() const { return Disc; }
    const TArray<FDiscTrajectorySample>& GetTrajectorySamples() const { return TrajectorySamples; }
    const TArray<FDiscGroundTransition>& GetGroundTransitions() const { return GroundTransitions; }
    double GetPendingSimulationTimeSeconds() const { return Accumulator; }
    /** Diagnostic-only latch; it never changes solver or gameplay authority. */
    bool DidLastFlightTerminateInvalidly() const { return bLastFlightTerminatedInvalidly; }
    const FString& GetLastFlightValidationFailureContext() const
    {
        return LastFlightValidationFailureContext;
    }
    const FString& GetLastFlightValidationFailure() const
    {
        return LastFlightValidationFailure;
    }

private:
    struct FSimulationConfigurationSnapshot
    {
        float AirDensityKgM3 = 0.0f;
        float FixedStepSeconds = 0.0f;
        float MaxFlightSeconds = 0.0f;
        float MaxGroundPlaySeconds = 0.0f;
        float MaxPrecessionRateRadPerSec = 0.0f;
        float GroundContactBiasMps = 0.0f;
        float GroundProbeDepthCm = 0.0f;
        float TrajectorySampleHz = 0.0f;
        TWeakObjectPtr<AWindDirector> PhysicsWindDirector;
        FDiscGolfPhysicsWindConfigurationSnapshot PhysicsWindConfiguration;
        bool bHasPhysicsWindDirector = false;
        bool bCaptured = false;
    };

    UPROPERTY() TObjectPtr<AWindDirector> WindDirector;
    FResolvedDiscDefinition Disc;
    bool bDiscConfigured = false;
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
    FTransform LastAuthoritativeOwnerTransform = FTransform::Identity;
    bool bHasAuthoritativeOwnerTransform = false;
    FVector LastValidPhysicsWindMps = FVector::ZeroVector;
    double Accumulator = 0.0;
    int32 ConsecutiveSkipCount = 0;
    TArray<FDiscTrajectorySample> TrajectorySamples;
    TArray<FDiscGroundTransition> GroundTransitions;
    TMap<TWeakObjectPtr<AActor>, float> VegetationOverlapTimes;
    float NextTrajectorySampleTime = 0.0f;
    bool bTrajectorySampleScheduleInitialized = false;
    FSimulationConfigurationSnapshot LaunchSimulationConfiguration;
    bool bLastFlightTerminatedInvalidly = false;
    FString LastFlightValidationFailureContext;
    FString LastFlightValidationFailure;

    void SimulateFixedStep(float Dt);
    void SimulateAirborneFixedStep(float Dt);
    void SimulateGroundFixedStep(float Dt);
    void TransitionGroundSupportLossToAirborne(float Dt);
    FVector ComputeAeroForceN(const FVector& AirVelocityMps, float& OutAlphaRad, FVector& OutUPlane, FVector& OutULateral) const;
    FVector ComputeAeroTorqueWorldNm(const FVector& AirVelocityMps, float AlphaRad, const FVector& UPlane, const FVector& ULateral) const;
    void IntegrateAttitude(const FVector& TorqueWorldNm, float Dt);
    void ResolveHit(const FHitResult& Hit);
    void ResolveGroundHit(const FHitResult& Hit);
    void ResolveObstacleHit(const FHitResult& Hit);
    bool ProbeGroundSupport(FHitResult& OutHit) const;
    ECourseSurfaceType ResolveCourseSurface(const FHitResult& Hit) const;
    bool SetGroundContact(const FHitResult& Hit);
    void UpdateTelemetry(float AlphaRad);
    void RebuildBodyRotation(const FVector& PreferredForward);
    void RecordTrajectorySample(bool bForce = false);
    void RecordGroundTransition(
        EDiscGroundState FromState,
        EDiscGroundState ToState,
        float ImpactSpeedMps = 0.0f,
        float IncidenceAngleDeg = 0.0f,
        float DiscEdgeAngleDeg = 0.0f);
    bool ValidateCurrentFlightState(FString& OutError) const;
    bool ValidateLaunchSimulationConfiguration(FString& OutError) const;
    void TerminateInvalidFlight(const FString& Context, const FString& Error);
};
