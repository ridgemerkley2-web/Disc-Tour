#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAudioTypes.generated.h"

UENUM(BlueprintType)
enum class EDGAudioEventType : uint8
{
    ThrowRelease,
    DiscFlightLoop,
    DiscImpact,
    BasketChains,
    BasketCage,
    CrowdReaction,
    EnvironmentAmbience
};

UENUM(BlueprintType)
enum class EDGImpactSurface : uint8
{
    Unknown,
    Grass,
    Dirt,
    Rock,
    Water,
    Chain,
    Basket,
    OutOfBounds
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGAudioEventPayload
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGAudioEventType EventType = EDGAudioEventType::EnvironmentAmbience;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector WorldLocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpeedMps = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpinRpm = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WobbleDegrees = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGImpactSurface Surface = EDGImpactSurface::Unknown;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Wetness01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Intensity01 = 1.0f;
};
