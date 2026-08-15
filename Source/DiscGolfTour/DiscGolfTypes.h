#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.generated.h"

UENUM(BlueprintType)
enum class EDiscPlastic : uint8
{
    Base,
    Tour,
    Crystal
};

UENUM(BlueprintType)
enum class EThrowStyle : uint8
{
    Backhand,
    Forehand
};

UENUM(BlueprintType)
enum class EDiscShotContext : uint8
{
    Drive,
    Circle2Putt,
    Circle1Putt
};

UENUM(BlueprintType)
enum class EBasketContactResult : uint8
{
    None,
    Caught,
    ChainDeflection,
    BandRejection,
    TrayRejection
};

UENUM(BlueprintType)
enum class EReleaseGrade : uint8
{
    Perfect,
    Great,
    Good,
    Poor
};

UENUM(BlueprintType)
enum class EReleaseTiming : uint8
{
    Early,
    OnTime,
    Late
};

UENUM(BlueprintType)
enum class EDiscFlightState : uint8
{
    Idle,
    Flying,
    GroundPlay,
    Settled,
    HoledOut
};

UENUM(BlueprintType)
enum class EDiscGroundState : uint8
{
    Airborne,
    Impact,
    Skipping,
    Sliding,
    EdgeRolling,
    Settled
};

UENUM(BlueprintType)
enum class EGroundSurfaceType : uint8
{
    Fairway,
    Rough,
    Dirt,
    Rock,
    TeePad
};

/** Authoritative collision identity for discrete world fixtures, independent of visual meshes. */
UENUM(BlueprintType)
enum class EDiscGolfFixtureType : uint8
{
    Unknown,
    Tree,
    DenseGrass,
    Rock,
    Sign
};

UENUM(BlueprintType)
enum class ELieType : uint8
{
    Tee,
    Fairway,
    LightRough,
    DeepRough,
    Circle2,
    Circle1,
    Hazard
};

/** Gameplay identity of the course at a point, independent of visual mesh names. */
UENUM(BlueprintType)
enum class ECourseSurfaceType : uint8
{
    Fairway,
    TeePad,
    LightRough,
    DeepRough,
    Dirt,
    Rock,
    OutOfBounds,
    Hazard
};

UENUM(BlueprintType)
enum class EDiscGolfPenaltyType : uint8
{
    None,
    OutOfBounds,
    Hazard
};

UENUM(BlueprintType)
enum class EDiscGolfReliefRule : uint8
{
    PlayFromResult,
    LastInBounds
};

/** Deterministic command modifiers owned by lie rules, not the flight solver. */
USTRUCT(BlueprintType)
struct FLieEffectProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame) FName ProfileId = TEXT("Clean");
    UPROPERTY(BlueprintReadOnly, SaveGame) float PowerMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly, SaveGame) float TimingErrorMultiplier = 1.0f;
};

/** Serializable result of applying course rules to one completed throw. */
USTRUCT(BlueprintType)
struct FDiscGolfLieState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame) int32 SchemaVersion = 1;
    UPROPERTY(BlueprintReadOnly, SaveGame) ECourseSurfaceType SurfaceAtRest = ECourseSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly, SaveGame) ECourseSurfaceType PlayingSurface = ECourseSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly, SaveGame) ELieType LieType = ELieType::Tee;
    UPROPERTY(BlueprintReadOnly, SaveGame) EDiscShotContext ShotContext = EDiscShotContext::Drive;
    UPROPERTY(BlueprintReadOnly, SaveGame) EDiscGolfPenaltyType PenaltyType = EDiscGolfPenaltyType::None;
    UPROPERTY(BlueprintReadOnly, SaveGame) EDiscGolfReliefRule ReliefRule = EDiscGolfReliefRule::PlayFromResult;
    UPROPERTY(BlueprintReadOnly, SaveGame) FVector RawDiscLocationCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, SaveGame) FVector LieLocationCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly, SaveGame) float DistanceToBasketMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly, SaveGame) int32 PenaltyStrokes = 0;
    UPROPERTY(BlueprintReadOnly, SaveGame) FLieEffectProfile Effects;
};

USTRUCT(BlueprintType)
struct FDiscAeroProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry") float MassKg = 0.175f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry") float DiameterM = 0.211f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry") float AreaM2 = 0.03496f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry") float InertiaAxialKgM2 = 0.000974f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry") float InertiaPlanarKgM2 = 0.000487f;

    // Baseline flying-disc coefficient family. These are calibration seeds, not claims about a specific mold.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics") float CL0 = 0.28f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics") float CLa = 1.80f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics") float CD0 = 0.095f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics") float CDa = 0.62f;
    // Reserved trim/precession terms. v0.1 keeps these neutral until measured golf-disc trajectories are available.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics") float CM0 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics") float CMa = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Calibration") float HighSpeedTurnMomentNm = 0.0035f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Calibration") float LowSpeedFadeMomentNm = 0.0025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Calibration") float TurnStartsAboveMps = 18.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Calibration") float FadeStartsBelowMps = 16.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Calibration") float SpinDecayPerSecond = 0.032f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground") float GroundRestitution = 0.16f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground") float GroundFriction = 0.46f;
};

USTRUCT(BlueprintType)
struct FDiscMoldDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MoldId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Speed = 7;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Glide = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Turn = -1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Fade = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDiscAeroProfile Aero;
};

/** Data-driven modifiers applied after a mold is resolved. */
USTRUCT(BlueprintType)
struct FDiscPlasticDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PlasticId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDiscPlastic Plastic = EDiscPlastic::Tour;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HighSpeedTurnMomentScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LowSpeedFadeMomentScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GroundRestitutionScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GroundFrictionScale = 1.0f;
};

USTRUCT(BlueprintType)
struct FResolvedDiscDefinition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName MoldId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    UPROPERTY(BlueprintReadOnly) int32 Speed = 7;
    UPROPERTY(BlueprintReadOnly) int32 Glide = 5;
    UPROPERTY(BlueprintReadOnly) float Turn = -1.0f;
    UPROPERTY(BlueprintReadOnly) float Fade = 2.0f;
    UPROPERTY(BlueprintReadOnly) EDiscPlastic Plastic = EDiscPlastic::Tour;
    UPROPERTY(BlueprintReadOnly) FDiscAeroProfile Aero;
};

USTRUCT(BlueprintType)
struct FThrowCommand
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FName MoldId = NAME_None;
    UPROPERTY(BlueprintReadWrite) EDiscPlastic Plastic = EDiscPlastic::Tour;
    UPROPERTY(BlueprintReadWrite) EThrowStyle ThrowStyle = EThrowStyle::Backhand;
    UPROPERTY(BlueprintReadWrite) EDiscShotContext ShotContext = EDiscShotContext::Drive;
    UPROPERTY(BlueprintReadWrite) FVector Direction = FVector::ForwardVector;
    UPROPERTY(BlueprintReadWrite) float Power01 = 0.82f;
    UPROPERTY(BlueprintReadWrite) float HyzerDeg = 3.0f;
    UPROPERTY(BlueprintReadWrite) float NoseAngleDeg = 1.0f;
    UPROPERTY(BlueprintReadWrite) float LaunchAngleDeg = 7.0f;
    UPROPERTY(BlueprintReadWrite) float TimingError = 0.0f;
};

/**
 * Immutable, deterministic launch state produced from one player throw command.
 * The flight solver consumes this result; it does not reinterpret timing input.
 */
USTRUCT(BlueprintType)
struct FThrowRelease
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EReleaseGrade Grade = EReleaseGrade::Perfect;
    UPROPERTY(BlueprintReadOnly) EReleaseTiming Timing = EReleaseTiming::OnTime;
    UPROPERTY(BlueprintReadOnly) float TimingError = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Quality01 = 1.0f;
    UPROPERTY(BlueprintReadOnly) float SpeedMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float SpinMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float ReleaseSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float SpinRpm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AimOffsetDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float HyzerOffsetDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float NoseOffsetDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LaunchOffsetDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float EffectiveHyzerDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float EffectiveNoseAngleDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float EffectiveLaunchAngleDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) EThrowStyle ThrowStyle = EThrowStyle::Backhand;
    UPROPERTY(BlueprintReadOnly) EDiscShotContext ShotContext = EDiscShotContext::Drive;
    UPROPERTY(BlueprintReadOnly) FVector Direction = FVector::ForwardVector;
    UPROPERTY(BlueprintReadOnly) float LiePowerMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float LieTimingErrorMultiplier = 1.0f;
};

/** Deterministic approximation of the short interaction between a disc and a basket assembly. */
USTRUCT(BlueprintType)
struct FBasketContactEvaluation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EBasketContactResult Result = EBasketContactResult::None;
    UPROPERTY(BlueprintReadOnly) float PredictedRadialCm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float PredictedHeightCm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float IncomingSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) FVector DeflectedVelocityMps = FVector::ZeroVector;
};

/** Deterministic impulse contract for a discrete world fixture. */
USTRUCT(BlueprintType)
struct FDiscGolfFixtureImpactProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EDiscGolfFixtureType FixtureType = EDiscGolfFixtureType::Unknown;
    UPROPERTY(BlueprintReadOnly) float NormalRestitution = 0.07f;
    UPROPERTY(BlueprintReadOnly) float TangentialRetention = 0.16f;
    UPROPERTY(BlueprintReadOnly) float SpinRetention = 0.78f;
    UPROPERTY(BlueprintReadOnly) float PassThroughSpeedRetention = 1.0f;
    UPROPERTY(BlueprintReadOnly) bool bOverlapVolume = false;
};

USTRUCT(BlueprintType)
struct FDiscGolfFixtureImpactResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EDiscGolfFixtureType FixtureType = EDiscGolfFixtureType::Unknown;
    UPROPERTY(BlueprintReadOnly) FVector VelocityMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float SpinMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float ImpactSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float NormalSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) bool bPassThrough = false;
};

USTRUCT(BlueprintType)
struct FGroundSurfaceProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EGroundSurfaceType Surface = EGroundSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) float RestitutionScale = 1.0f;
    UPROPERTY(BlueprintReadOnly) float FrictionScale = 1.0f;
    UPROPERTY(BlueprintReadOnly) float ImpactSpinRetention = 0.88f;
    UPROPERTY(BlueprintReadOnly) float SkipMinSpeedMps = 7.5f;
    UPROPERTY(BlueprintReadOnly) float SkipMaxIncidenceDeg = 19.0f;
    UPROPERTY(BlueprintReadOnly) int32 MaxConsecutiveSkips = 3;
    UPROPERTY(BlueprintReadOnly) float EdgeRollMinAngleDeg = 55.0f;
    UPROPERTY(BlueprintReadOnly) float EdgeRollMinSpeedMps = 3.0f;
    UPROPERTY(BlueprintReadOnly) float EdgeRollMinSpinRpm = 150.0f;
    UPROPERTY(BlueprintReadOnly) float SlideDecelerationMps2 = 2.2f;
    UPROPERTY(BlueprintReadOnly) float RollDecelerationMps2 = 1.15f;
    UPROPERTY(BlueprintReadOnly) float GroundSpinDecayPerSecond = 1.25f;
    UPROPERTY(BlueprintReadOnly) float SettleSpeedMps = 1.1f;
};

USTRUCT(BlueprintType)
struct FGroundImpactResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EDiscGroundState State = EDiscGroundState::Impact;
    UPROPERTY(BlueprintReadOnly) EGroundSurfaceType Surface = EGroundSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) FVector VelocityMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float SpinMultiplier = 1.0f;
    UPROPERTY(BlueprintReadOnly) float ImpactSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ApproachSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float TangentialSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float IncidenceAngleDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float DiscEdgeAngleDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float EffectiveRestitution = 0.0f;
    UPROPERTY(BlueprintReadOnly) float EffectiveFriction = 0.0f;
};

USTRUCT(BlueprintType)
struct FDiscTrajectorySample
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float TimeSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) FVector WorldLocationCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector VelocityMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector DiscNormalWorld = FVector::UpVector;
    UPROPERTY(BlueprintReadOnly) FVector WindMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float SpinRpm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AngleOfAttackDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) EDiscGroundState GroundState = EDiscGroundState::Airborne;
    UPROPERTY(BlueprintReadOnly) EGroundSurfaceType GroundSurface = EGroundSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) ECourseSurfaceType CourseSurface = ECourseSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) int32 GroundContactCount = 0;
};

/** Discrete state boundary retained alongside fixed-rate trajectory samples. */
USTRUCT(BlueprintType)
struct FDiscGroundTransition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float TimeSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) FVector WorldLocationCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) EDiscGroundState FromState = EDiscGroundState::Airborne;
    UPROPERTY(BlueprintReadOnly) EDiscGroundState ToState = EDiscGroundState::Impact;
    UPROPERTY(BlueprintReadOnly) EGroundSurfaceType Surface = EGroundSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) ECourseSurfaceType CourseSurface = ECourseSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) int32 GroundContactCount = 0;
    UPROPERTY(BlueprintReadOnly) float ImpactSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float IncidenceAngleDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float DiscEdgeAngleDeg = 0.0f;
};

USTRUCT(BlueprintType)
struct FDiscFlightTelemetry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EDiscFlightState State = EDiscFlightState::Idle;
    UPROPERTY(BlueprintReadOnly) FVector VelocityMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float SpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float SpinRpm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AngleOfAttackDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float FlightTimeSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float CarryMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) EDiscGroundState GroundState = EDiscGroundState::Airborne;
    UPROPERTY(BlueprintReadOnly) EGroundSurfaceType GroundSurface = EGroundSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) ECourseSurfaceType CourseSurface = ECourseSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) int32 GroundContactCount = 0;
    UPROPERTY(BlueprintReadOnly) float GroundPlayTimeSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float GroundDistanceMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LastImpactSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LastImpactIncidenceDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LastDiscEdgeAngleDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 FixtureContactCount = 0;
    UPROPERTY(BlueprintReadOnly) EDiscGolfFixtureType LastFixtureType = EDiscGolfFixtureType::Unknown;
    UPROPERTY(BlueprintReadOnly) float LastFixtureImpactSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) FVector LastFixtureEntryVelocityMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector LastFixtureExitVelocityMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector LastFixtureImpactNormal = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float LastFixtureEntrySpinRpm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LastFixtureExitSpinRpm = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 BasketContactCount = 0;
    UPROPERTY(BlueprintReadOnly) EBasketContactResult LastBasketContact = EBasketContactResult::None;
    UPROPERTY(BlueprintReadOnly) FThrowRelease Release;
};
