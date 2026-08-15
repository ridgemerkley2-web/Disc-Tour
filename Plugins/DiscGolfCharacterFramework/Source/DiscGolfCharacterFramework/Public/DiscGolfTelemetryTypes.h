#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfTelemetryTypes.generated.h"

UENUM(BlueprintType)
enum class EDGImpactSurface : uint8
{
    Unknown,
    Grass,
    Dirt,
    Rock,
    Tree,
    Branch,
    Basket,
    Chain,
    Water,
    OutOfBounds
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGThrowReleaseTelemetry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Release")
    FDGReleaseData ReleaseData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Release")
    FDGDiscInstance Disc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Release")
    FVector WindVelocityMps = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Release")
    float WobbleDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Release")
    float LaunchElevationDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGFlightSample
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight")
    float TimeSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight")
    FVector WorldLocationCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight")
    FVector LinearVelocityMps = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight")
    FVector AngularVelocityRadPerSec = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight")
    FRotator Orientation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGImpactEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
    float TimeSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
    EDGImpactSurface Surface = EDGImpactSurface::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
    FVector WorldLocationCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
    FVector SurfaceNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact")
    float SpeedMps = 0.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGThrowSummary
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    float ApexHeightM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    float CarryDistanceM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    float TotalDistanceM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    float GroundPlayDistanceM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    float FlightTimeSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    float MaximumSpeedMps = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    float LateralDeviationM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    bool bHoledOut = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Summary")
    bool bOutOfBounds = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGThrowTelemetryRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    FGuid RecordId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    FDateTime RecordedUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    FName CourseId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    int32 HoleNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    FDGThrowReleaseTelemetry Release;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    TArray<FDGFlightSample> Samples;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    TArray<FDGImpactEvent> Impacts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    FDGThrowSummary Summary;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw")
    TArray<FName> Tags;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGShotReplayFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Replay")
    float PlaybackTimeSeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Replay")
    FVector WorldLocationCm = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Replay")
    FRotator Orientation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category="Replay")
    FVector LinearVelocityMps = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Replay")
    bool bFinished = false;
};
