#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTelemetryTypes.h"
#include "DiscGolfAudioTypes.generated.h"

class USoundBase;

UENUM(BlueprintType)
enum class EDGAudioEventType : uint8
{
    ThrowRelease,
    DiscFlightLoop,
    DiscImpact,
    BasketChains,
    BasketCage,
    Footstep,
    CrowdReaction,
    EnvironmentAmbience
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAudioEventPayload
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    EDGAudioEventType EventType = EDGAudioEventType::ThrowRelease;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    FVector WorldLocationCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    float SpeedMps = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    float SpinRpm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    float WobbleDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    EDGImpactSurface Surface = EDGImpactSurface::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    float Wetness01 = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    float Intensity01 = 1.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAudioFallbackEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    EDGAudioEventType EventType = EDGAudioEventType::ThrowRelease;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TSoftObjectPtr<USoundBase> Sound;
};
