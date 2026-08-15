#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DiscTrajectoryTypes.h"
#include "DiscTrajectorySubsystem.generated.h"

UCLASS()
class DISCGOLFTOUR_API UDiscTrajectorySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    const TArray<FPhysicsRegressionPreset>& GetRegressionPresets() const { return RegressionPresets; }
    const FString& GetExportDirectory() const { return ExportDirectory; }
    const FString& GetLastExportJsonPath() const { return LastExportJsonPath; }
    const FString& GetLastExportCsvPath() const { return LastExportCsvPath; }
    bool HasLastCapture() const { return bHasLastCapture; }
    const FDiscTrajectorySummary& GetLastSummary() const { return LastSummary; }

    bool CompleteCapture(
        const FResolvedDiscDefinition& Disc,
        const FThrowRelease& Release,
        const TArray<FDiscTrajectorySample>& Samples,
        const TArray<FDiscGroundTransition>& Transitions,
        const FDiscFlightTelemetry& Telemetry,
        const FDiscGolfLieState& ResultingLie,
        bool bHoledOut,
        FName PresetId,
        int32 RenderFps,
        FDiscTrajectorySummary& OutSummary,
        FString& OutError);

    bool WriteRegressionSuiteReport(
        const TArray<FDiscTrajectorySummary>& Summaries,
        FString& OutReportPath,
        bool& OutPassed,
        FString& OutError) const;

    static FDiscTrajectorySummary BuildSummary(
        const TArray<FDiscTrajectorySample>& Samples,
        const TArray<FDiscGroundTransition>& Transitions,
        const FDiscFlightTelemetry& Telemetry,
        const FThrowRelease& Release,
        const FDiscGolfLieState& ResultingLie,
        bool bHoledOut,
        FName PresetId,
        int32 RenderFps);

    static bool EvaluateRegression(
        const FPhysicsRegressionPreset& Preset,
        FDiscTrajectorySummary& InOutSummary);

    static FString BuildCsv(
        const FResolvedDiscDefinition& Disc,
        const FThrowRelease& Release,
        const TArray<FDiscTrajectorySample>& Samples,
        const TArray<FDiscGroundTransition>& Transitions,
        const FDiscTrajectorySummary& Summary);

    static FString BuildJson(
        const FResolvedDiscDefinition& Disc,
        const FThrowRelease& Release,
        const TArray<FDiscTrajectorySample>& Samples,
        const TArray<FDiscGroundTransition>& Transitions,
        const FDiscFlightTelemetry& Telemetry,
        const FDiscTrajectorySummary& Summary);

private:
    TArray<FPhysicsRegressionPreset> RegressionPresets;
    FString ExportDirectory;
    FString LastExportJsonPath;
    FString LastExportCsvPath;
    FDiscTrajectorySummary LastSummary;
    bool bHasLastCapture = false;

    bool LoadRegressionPresets(FString& OutError);
    void BuildFallbackPresets();
};
