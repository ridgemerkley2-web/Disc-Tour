#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfAITypes.generated.h"

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAIGolferSkillProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Power = 0.70f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Accuracy = 0.70f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Putting = 0.70f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Consistency = 0.70f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Aggression = 0.50f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float CourseManagement = 0.70f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAIShotContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    FVector LieLocationCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    FVector TargetLocationCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    FVector WindVelocityMps = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    bool bPutting = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    int32 StrokesTaken = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    int32 Par = 3;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAIShotCandidate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    FDGDiscInstance Disc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    EDGThrowType ThrowType = EDGThrowType::Backhand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    float Power01 = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    float HyzerDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    float NoseDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    FVector PredictedLandingLocationCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    float ExpectedProgressM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SuccessProbability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float OutOfBoundsProbability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ObstacleHitProbability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    float LandingErrorM = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="AI")
    float UtilityScore = 0.0f;
};
