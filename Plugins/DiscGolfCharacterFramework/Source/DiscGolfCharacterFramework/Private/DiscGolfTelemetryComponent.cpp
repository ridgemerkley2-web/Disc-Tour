#include "DiscGolfTelemetryComponent.h"
#include "Engine/World.h"

UDiscGolfTelemetryComponent::UDiscGolfTelemetryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

float UDiscGolfTelemetryComponent::GetActiveElapsedSeconds() const
{
    if (!bRecordingThrow || !GetWorld())
    {
        return 0.0f;
    }

    return static_cast<float>(GetWorld()->GetTimeSeconds() - StartWorldTimeSeconds);
}

void UDiscGolfTelemetryComponent::BeginThrow(
    const FDGReleaseData& ReleaseData,
    const FDGDiscInstance& Disc,
    FVector WindVelocityMps,
    float WobbleDegrees,
    float LaunchElevationDegrees,
    FName CourseId,
    int32 HoleNumber)
{
    ActiveRecord = FDGThrowTelemetryRecord();
    ActiveRecord.RecordId = FGuid::NewGuid();
    ActiveRecord.RecordedUtc = FDateTime::UtcNow();
    ActiveRecord.CourseId = CourseId;
    ActiveRecord.HoleNumber = HoleNumber;
    ActiveRecord.Release.ReleaseData = ReleaseData;
    ActiveRecord.Release.Disc = Disc;
    ActiveRecord.Release.WindVelocityMps = WindVelocityMps;
    ActiveRecord.Release.WobbleDegrees = WobbleDegrees;
    ActiveRecord.Release.LaunchElevationDegrees = LaunchElevationDegrees;

    ReleaseLocationCm = ReleaseData.GripWorldTransform.GetLocation();
    StartWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    LastSampleTimeSeconds = -1.0f;
    bCarryCaptured = false;
    bRecordingThrow = true;
}

void UDiscGolfTelemetryComponent::RecordFlightSample(
    FVector WorldLocationCm,
    FVector LinearVelocityCmPerSec,
    FVector AngularVelocityRadPerSec,
    FRotator Orientation)
{
    if (!bRecordingThrow || ActiveRecord.Samples.Num() >= FMath::Max(1, MaxSamplesPerThrow))
    {
        return;
    }

    const float Elapsed = GetActiveElapsedSeconds();

    if (LastSampleTimeSeconds >= 0.0f &&
        Elapsed - LastSampleTimeSeconds < FMath::Max(0.0f, MinimumSampleIntervalSeconds))
    {
        return;
    }

    FDGFlightSample Sample;
    Sample.TimeSeconds = Elapsed;
    Sample.WorldLocationCm = WorldLocationCm;
    Sample.LinearVelocityMps = LinearVelocityCmPerSec / 100.0f;
    Sample.AngularVelocityRadPerSec = AngularVelocityRadPerSec;
    Sample.Orientation = Orientation;
    ActiveRecord.Samples.Add(Sample);

    LastSampleTimeSeconds = Elapsed;

    const float HeightM = (WorldLocationCm.Z - ReleaseLocationCm.Z) / 100.0f;
    ActiveRecord.Summary.ApexHeightM = FMath::Max(ActiveRecord.Summary.ApexHeightM, HeightM);
    ActiveRecord.Summary.MaximumSpeedMps = FMath::Max(
        ActiveRecord.Summary.MaximumSpeedMps,
        Sample.LinearVelocityMps.Size()
    );
}

void UDiscGolfTelemetryComponent::RecordImpact(
    EDGImpactSurface Surface,
    FVector WorldLocationCm,
    FVector SurfaceNormal,
    float SpeedMps)
{
    if (!bRecordingThrow)
    {
        return;
    }

    FDGImpactEvent Impact;
    Impact.TimeSeconds = GetActiveElapsedSeconds();
    Impact.Surface = Surface;
    Impact.WorldLocationCm = WorldLocationCm;
    Impact.SurfaceNormal = SurfaceNormal;
    Impact.SpeedMps = SpeedMps;
    ActiveRecord.Impacts.Add(Impact);

    if (!bCarryCaptured &&
        (Surface == EDGImpactSurface::Grass ||
         Surface == EDGImpactSurface::Dirt ||
         Surface == EDGImpactSurface::Rock))
    {
        const FVector Delta = WorldLocationCm - ReleaseLocationCm;
        ActiveRecord.Summary.CarryDistanceM = FVector(Delta.X, Delta.Y, 0.0f).Size() / 100.0f;
        bCarryCaptured = true;
    }
}

bool UDiscGolfTelemetryComponent::FinishThrow(
    FVector FinalWorldLocationCm,
    bool bHoledOut,
    bool bOutOfBounds)
{
    if (!bRecordingThrow)
    {
        return false;
    }

    ActiveRecord.Summary.FlightTimeSeconds = GetActiveElapsedSeconds();
    ActiveRecord.Summary.bHoledOut = bHoledOut;
    ActiveRecord.Summary.bOutOfBounds = bOutOfBounds;

    const FVector Delta = FinalWorldLocationCm - ReleaseLocationCm;
    ActiveRecord.Summary.TotalDistanceM = FVector(Delta.X, Delta.Y, 0.0f).Size() / 100.0f;

    if (!bCarryCaptured)
    {
        ActiveRecord.Summary.CarryDistanceM = ActiveRecord.Summary.TotalDistanceM;
    }

    ActiveRecord.Summary.GroundPlayDistanceM = FMath::Max(
        0.0f,
        ActiveRecord.Summary.TotalDistanceM - ActiveRecord.Summary.CarryDistanceM
    );

    const FVector Forward = ActiveRecord.Release.ReleaseData.GripWorldTransform.GetRotation().GetForwardVector();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    ActiveRecord.Summary.LateralDeviationM = FVector::DotProduct(Delta, Right) / 100.0f;

    CompletedRecords.Add(ActiveRecord);

    while (CompletedRecords.Num() > FMath::Max(1, MaxStoredRecords))
    {
        CompletedRecords.RemoveAt(0);
    }

    bRecordingThrow = false;
    OnThrowTelemetryCompleted.Broadcast(ActiveRecord);
    return true;
}

void UDiscGolfTelemetryComponent::CancelThrow()
{
    bRecordingThrow = false;
    ActiveRecord = FDGThrowTelemetryRecord();
    LastSampleTimeSeconds = -1.0f;
    bCarryCaptured = false;
}

void UDiscGolfTelemetryComponent::ClearCompletedRecords()
{
    CompletedRecords.Reset();
}

bool UDiscGolfTelemetryComponent::GetLatestRecord(FDGThrowTelemetryRecord& OutRecord) const
{
    if (CompletedRecords.Num() <= 0)
    {
        return false;
    }

    OutRecord = CompletedRecords.Last();
    return true;
}
