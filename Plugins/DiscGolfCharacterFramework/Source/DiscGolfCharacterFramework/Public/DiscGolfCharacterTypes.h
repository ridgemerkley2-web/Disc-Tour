#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.generated.h"

UENUM(BlueprintType)
enum class EDGHandedness : uint8
{
    Right UMETA(DisplayName="Right Handed"),
    Left  UMETA(DisplayName="Left Handed")
};

UENUM(BlueprintType)
enum class EDGThrowType : uint8
{
    Backhand,
    Forehand,
    Standstill,
    Putt
};

UENUM(BlueprintType)
enum class EDGThrowPhase : uint8
{
    Idle,
    Aim,
    RunUp,
    ReachBack,
    Plant,
    Acceleration,
    Release,
    FollowThrough,
    Recovery
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGBodyProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body", meta=(ClampMin="150.0", ClampMax="210.0"))
    float HeightCm = 183.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body", meta=(ClampMin="0.90", ClampMax="1.10"))
    float WingspanScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body", meta=(ClampMin="0.90", ClampMax="1.10"))
    float ShoulderWidthScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body", meta=(ClampMin="0.92", ClampMax="1.08"))
    float TorsoLengthScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body", meta=(ClampMin="0.92", ClampMax="1.08"))
    float LegLengthScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body", meta=(ClampMin="0.90", ClampMax="1.10"))
    float HandScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Body", meta=(ClampMin="45.0", ClampMax="160.0"))
    float MassKg = 82.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGThrowStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw Style", meta=(ClampMin="0.0", ClampMax="1.0"))
    float RunUpIntensity = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw Style", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ReachBackAmount = 0.80f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw Style", meta=(ClampMin="0.0", ClampMax="1.0"))
    float TorsoRotation = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw Style", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BraceIntensity = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw Style", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Explosiveness = 0.60f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw Style", meta=(ClampMin="0.0", ClampMax="1.0"))
    float FollowThrough = 0.80f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw Style", meta=(ClampMin="0.75", ClampMax="1.25"))
    float PowerMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw Style", meta=(ClampMin="0.75", ClampMax="1.25"))
    float SpinMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGThrowCapability
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capability")
    float MinLaunchSpeedMps = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capability")
    float MaxLaunchSpeedMps = 32.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capability")
    float MinSpinRpm = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Capability")
    float MaxSpinRpm = 1400.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGThrowIntent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw")
    EDGThrowType ThrowType = EDGThrowType::Backhand;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Power01 = 0.75f;

    // Positive means hyzer relative to throwing hand. Negative means anhyzer.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw", meta=(ClampMin="-60.0", ClampMax="60.0"))
    float HyzerDegrees = 0.0f;

    // Positive means nose-up.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw", meta=(ClampMin="-20.0", ClampMax="20.0"))
    float NoseDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw")
    float AimYawDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Throw", meta=(ClampMin="0.0", ClampMax="1.0"))
    float TimingQuality01 = 1.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGReleaseData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Release")
    FTransform GripWorldTransform = FTransform::Identity;

    UPROPERTY(BlueprintReadOnly, Category="Release")
    FDGThrowIntent Intent;

    UPROPERTY(BlueprintReadOnly, Category="Release")
    EDGHandedness Handedness = EDGHandedness::Right;

    UPROPERTY(BlueprintReadOnly, Category="Release")
    float SuggestedLaunchSpeedMps = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Release")
    float SuggestedSpinRpm = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Release")
    FVector GripLinearVelocityCmPerSec = FVector::ZeroVector;
};
