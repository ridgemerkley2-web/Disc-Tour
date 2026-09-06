#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"
#include "DiscTrajectoryTypes.h"
#include "DiscThrowLabTypes.generated.h"

UENUM(BlueprintType)
enum class EDiscThrowLabComparisonCompatibility : uint8
{
    NotConfigured,
    Compatible,
    MissingRecord,
    SameRecord,
    SchemaMismatch,
    ShotContextMismatch,
    RegressionModeMismatch,
    InvalidRecord
};

/**
 * Development-only value snapshot of one completed authoritative trajectory capture.
 * ReplaySamples are selected from the solver samples; Throw Lab never interpolates or
 * resimulates while recording.
 */
USTRUCT(BlueprintType)
struct FDiscThrowLabRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame) int32 SchemaVersion = 1;
    UPROPERTY(BlueprintReadOnly, SaveGame) FString RecordId;
    UPROPERTY(BlueprintReadOnly, SaveGame) FString CaptureId;
    UPROPERTY(BlueprintReadOnly, SaveGame) FString CapturedUtc;
    UPROPERTY(BlueprintReadOnly, SaveGame) bool bPinned = false;

    UPROPERTY(BlueprintReadOnly, SaveGame) FResolvedDiscDefinition Disc;
    UPROPERTY(BlueprintReadOnly, SaveGame) FThrowRelease Release;
    UPROPERTY(BlueprintReadOnly, SaveGame) FDiscFlightTelemetry FinalTelemetry;
    UPROPERTY(BlueprintReadOnly, SaveGame) FDiscTrajectorySummary Summary;
    UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FDiscGroundTransition> GroundTransitions;
    UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FDiscTrajectorySample> ReplaySamples;

    UPROPERTY(BlueprintReadOnly, SaveGame) int32 SourceSampleCount = 0;
    UPROPERTY(BlueprintReadOnly, SaveGame) float ReplayDurationSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly, SaveGame) float NominalReplaySampleRateHz = 0.0f;

    /** Explicit unit labels keep consumers from treating Unreal positions as SI distances. */
    UPROPERTY(BlueprintReadOnly, SaveGame) FString PositionUnit = TEXT("centimeter");
    UPROPERTY(BlueprintReadOnly, SaveGame) FString VelocityUnit = TEXT("meter_per_second");
    UPROPERTY(BlueprintReadOnly, SaveGame) FString SpinUnit = TEXT("revolution_per_minute");
};

/** Exact two-record comparison result. Incompatible pairs never expose metric deltas. */
USTRUCT(BlueprintType)
struct FDiscThrowLabComparison
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) EDiscThrowLabComparisonCompatibility Compatibility =
        EDiscThrowLabComparisonCompatibility::NotConfigured;
    UPROPERTY(BlueprintReadOnly) FString Reason;
    UPROPERTY(BlueprintReadOnly) FString FirstRecordId;
    UPROPERTY(BlueprintReadOnly) FString SecondRecordId;

    /** Second record minus first record. */
    UPROPERTY(BlueprintReadOnly) float ReleaseSpeedDeltaMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ReleaseSpinDeltaRpm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AirCarryDeltaMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float FinalCarryDeltaMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ApexDeltaMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float AirTimeDeltaSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly) float LateralDeltaMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float GroundPlayDeltaMeters = 0.0f;
};
