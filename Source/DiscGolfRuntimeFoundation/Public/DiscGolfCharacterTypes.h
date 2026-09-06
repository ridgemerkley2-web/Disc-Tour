#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.generated.h"

UENUM(BlueprintType)
enum class EDGHandedness : uint8
{
    Right,
    Left
};

UENUM(BlueprintType)
enum class EDGThrowType : uint8
{
    Backhand,
    Forehand
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

UENUM(BlueprintType)
enum class EDGBodyRegion : uint8
{
    Head,
    Hair,
    Torso,
    Arms,
    Hands,
    Legs,
    Feet
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGBodyProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float HeightCm = 183.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float WingspanScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float ShoulderWidthScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float TorsoLengthScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float LegLengthScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float HandScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float MassKg = 82.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGBodyBuildProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Muscularity = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float BodyFat = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Chest = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Waist = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Hips = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Arms = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Legs = 0.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGThrowStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float RunUpIntensity = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float ReachBackAmount = 0.80f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float TorsoRotation = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float BraceIntensity = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Explosiveness = 0.60f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float FollowThrough = 0.80f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float PowerMultiplier = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float SpinMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGThrowIntent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGThrowType ThrowType = EDGThrowType::Backhand;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Power01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HyzerDegrees = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NoseDegrees = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimYawDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGReleaseData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform GripWorldTransform = FTransform::Identity;
};
