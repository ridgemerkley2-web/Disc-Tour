#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"
#include "DiscTrajectoryTypes.generated.h"

USTRUCT(BlueprintType)
struct FPhysicsRegressionBounds
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float MinAirCarryMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MaxAirCarryMeters = 10000.0f;
    UPROPERTY(BlueprintReadOnly) float MinFinalCarryMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MaxFinalCarryMeters = 10000.0f;
    UPROPERTY(BlueprintReadOnly) float MinApexMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MaxApexMeters = 10000.0f;
    UPROPERTY(BlueprintReadOnly) float MinAirTimeSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MaxAirTimeSeconds = 10000.0f;
    UPROPERTY(BlueprintReadOnly) float MinLateralMeters = -10000.0f;
    UPROPERTY(BlueprintReadOnly) float MaxLateralMeters = 10000.0f;
    UPROPERTY(BlueprintReadOnly) int32 MinGroundContacts = 0;
    UPROPERTY(BlueprintReadOnly) int32 MaxGroundContacts = 100000;
    UPROPERTY(BlueprintReadOnly) bool bRequireFinalGroundState = true;
    UPROPERTY(BlueprintReadOnly) EDiscGroundState ExpectedFinalGroundState = EDiscGroundState::Settled;
    /** -1 ignores the result, 0 requires a miss, 1 requires a make. */
    UPROPERTY(BlueprintReadOnly) int32 ExpectedHoledOut = -1;
    UPROPERTY(BlueprintReadOnly) bool bRequireBasketContact = false;
    UPROPERTY(BlueprintReadOnly) EBasketContactResult ExpectedBasketContact = EBasketContactResult::None;
    UPROPERTY(BlueprintReadOnly) bool bRequireNoRulesPenalty = true;
};

/** Human-readable, reusable command plus acceptance envelope. */
USTRUCT(BlueprintType)
struct FPhysicsRegressionPreset
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName PresetId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    UPROPERTY(BlueprintReadOnly) FString Description;
    UPROPERTY(BlueprintReadOnly) FName MoldId = TEXT("Apex");
    UPROPERTY(BlueprintReadOnly) EDiscPlastic Plastic = EDiscPlastic::Tour;
    UPROPERTY(BlueprintReadOnly) EThrowStyle ThrowStyle = EThrowStyle::Backhand;
    UPROPERTY(BlueprintReadOnly) EDiscShotContext ShotContext = EDiscShotContext::Drive;
    UPROPERTY(BlueprintReadOnly) float StartDistanceMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AimOffsetDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Power01 = 0.82f;
    UPROPERTY(BlueprintReadOnly) float HyzerDeg = 3.0f;
    UPROPERTY(BlueprintReadOnly) float NoseAngleDeg = 1.0f;
    UPROPERTY(BlueprintReadOnly) float LaunchAngleDeg = 7.0f;
    UPROPERTY(BlueprintReadOnly) float TimingError = 0.0f;
    UPROPERTY(BlueprintReadOnly) FVector WindMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) int32 RenderFps = 60;
    UPROPERTY(BlueprintReadOnly) FPhysicsRegressionBounds Expected;

    FThrowCommand MakeCommand(const FVector& Direction) const
    {
        FThrowCommand Command;
        Command.MoldId = MoldId;
        Command.Plastic = Plastic;
        Command.ThrowStyle = ThrowStyle;
        Command.ShotContext = ShotContext;
        Command.Direction = Direction;
        Command.Power01 = Power01;
        Command.HyzerDeg = HyzerDeg;
        Command.NoseAngleDeg = NoseAngleDeg;
        Command.LaunchAngleDeg = LaunchAngleDeg;
        Command.TimingError = TimingError;
        return Command;
    }
};

USTRUCT(BlueprintType)
struct FDiscTrajectorySummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString CaptureId;
    UPROPERTY(BlueprintReadOnly) FString CapturedUtc;
    UPROPERTY(BlueprintReadOnly) FName PresetId = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 RenderFps = 0;
    UPROPERTY(BlueprintReadOnly) int32 SampleCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 GroundTransitionCount = 0;
    UPROPERTY(BlueprintReadOnly) float DurationSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AirTimeSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AirCarryMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float FinalCarryMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ApexMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LateralMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) FVector StartWorldLocationCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector FinalWorldLocationCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) EDiscGroundState FinalGroundState = EDiscGroundState::Airborne;
    UPROPERTY(BlueprintReadOnly) EGroundSurfaceType FinalGroundSurface = EGroundSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) int32 GroundContactCount = 0;
    UPROPERTY(BlueprintReadOnly) float GroundDistanceMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 BasketContactCount = 0;
    UPROPERTY(BlueprintReadOnly) EBasketContactResult LastBasketContact = EBasketContactResult::None;
    UPROPERTY(BlueprintReadOnly) bool bHoledOut = false;
    UPROPERTY(BlueprintReadOnly) ECourseSurfaceType SurfaceAtRest = ECourseSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) ECourseSurfaceType PlayingSurface = ECourseSurfaceType::Fairway;
    UPROPERTY(BlueprintReadOnly) ELieType ResultingLieType = ELieType::Fairway;
    UPROPERTY(BlueprintReadOnly) EDiscGolfPenaltyType PenaltyType = EDiscGolfPenaltyType::None;
    UPROPERTY(BlueprintReadOnly) EDiscGolfReliefRule ReliefRule = EDiscGolfReliefRule::PlayFromResult;
    UPROPERTY(BlueprintReadOnly) int32 PenaltyStrokes = 0;
    UPROPERTY(BlueprintReadOnly) FVector ResultingLieLocationCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bWasRegression = false;
    UPROPERTY(BlueprintReadOnly) bool bRegressionPassed = false;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RegressionFailures;
};
