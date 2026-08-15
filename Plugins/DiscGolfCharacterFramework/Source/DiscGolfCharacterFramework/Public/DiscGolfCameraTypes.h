#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCameraTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EDGCameraMode : uint8
{
    CharacterCreator,
    Aim,
    Tee,
    FollowDisc,
    Landing,
    Basket,
    ShotReplay,
    FreeCamera
};

UENUM(BlueprintType)
enum class EDGReplayMode : uint8
{
    TelemetryShotReplay,
    EngineReplay
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGCameraRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    EDGCameraMode Mode = EDGCameraMode::Aim;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    TObjectPtr<AActor> PrimaryTarget = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    TObjectPtr<AActor> SecondaryTarget = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    FVector WorldFocusLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    float BlendTimeSeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    float FieldOfViewDegrees = 70.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    bool bLockHorizon = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
    bool bAllowPlayerOrbit = false;
};
