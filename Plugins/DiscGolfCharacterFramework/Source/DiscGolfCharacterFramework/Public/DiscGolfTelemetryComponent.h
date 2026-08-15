#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfTelemetryTypes.h"
#include "DiscGolfTelemetryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGOnThrowTelemetryCompleted,
    FDGThrowTelemetryRecord,
    Record
);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfTelemetryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfTelemetryComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Telemetry")
    int32 MaxStoredRecords = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Telemetry")
    int32 MaxSamplesPerThrow = 2400;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Telemetry")
    float MinimumSampleIntervalSeconds = 1.0f / 60.0f;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Telemetry")
    bool bRecordingThrow = false;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Telemetry")
    FDGThrowTelemetryRecord ActiveRecord;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Telemetry")
    TArray<FDGThrowTelemetryRecord> CompletedRecords;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Telemetry")
    FDGOnThrowTelemetryCompleted OnThrowTelemetryCompleted;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Telemetry")
    void BeginThrow(
        const FDGReleaseData& ReleaseData,
        const FDGDiscInstance& Disc,
        FVector WindVelocityMps,
        float WobbleDegrees,
        float LaunchElevationDegrees,
        FName CourseId,
        int32 HoleNumber
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Telemetry")
    void RecordFlightSample(
        FVector WorldLocationCm,
        FVector LinearVelocityCmPerSec,
        FVector AngularVelocityRadPerSec,
        FRotator Orientation
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Telemetry")
    void RecordImpact(
        EDGImpactSurface Surface,
        FVector WorldLocationCm,
        FVector SurfaceNormal,
        float SpeedMps
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Telemetry")
    bool FinishThrow(
        FVector FinalWorldLocationCm,
        bool bHoledOut,
        bool bOutOfBounds
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Telemetry")
    void CancelThrow();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Telemetry")
    void ClearCompletedRecords();

    UFUNCTION(BlueprintPure, Category="Disc Golf|Telemetry")
    bool GetLatestRecord(FDGThrowTelemetryRecord& OutRecord) const;

private:
    double StartWorldTimeSeconds = 0.0;
    float LastSampleTimeSeconds = -1.0f;
    FVector ReleaseLocationCm = FVector::ZeroVector;
    bool bCarryCaptured = false;

    float GetActiveElapsedSeconds() const;
};
