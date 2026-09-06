#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfAITypes.generated.h"

USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDGAISkillProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Power = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Accuracy = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Putting = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Consistency = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Aggression = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CourseManagement = 0.5f;
};

USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDGAIShotContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LieLocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector TargetLocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector WindVelocityMps = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 StrokesTaken = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Par = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPutting = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDGAIShotCandidate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGDiscInstance Disc;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGThrowType ThrowType = EDGThrowType::Backhand;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Power01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HyzerDegrees = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NoseDegrees = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector PredictedLandingLocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ExpectedProgressM = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SuccessProbability = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float OutOfBoundsProbability = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ObstacleHitProbability = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LandingErrorM = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float UtilityScore = 0.0f;
};
