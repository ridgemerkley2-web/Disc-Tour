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
    bool AreRegressionPresetsAuthoritative() const { return bRegressionPresetsAuthoritative; }
    const FString& GetRegressionPresetFileSha1() const { return RegressionPresetFileSha1; }

    /**
     * Arms a one-shot deferral for the next completed capture. Summary and
     * regression evaluation still complete synchronously; only serialization and
     * file writes are postponed. A pending capture must be flushed or discarded
     * before another deferral can be armed.
     */
    bool DeferNextCaptureExport(FString& OutError);
    bool FlushDeferredCaptureExport(
        FDiscTrajectorySummary& OutSummary,
        FString& OutError);
    bool DiscardDeferredCaptureExport();
    bool IsCaptureExportDeferralArmed() const { return bCaptureExportDeferralArmed; }
    bool HasPendingDeferredCaptureExport() const { return bHasPendingDeferredCaptureExport; }
    bool GetPendingDeferredCaptureSummary(FDiscTrajectorySummary& OutSummary) const;

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
        bool bPresentationTraceUnchanged,
        FString& OutReportPath,
        bool& OutPassed,
        FString& OutError) const;

    /** Publishes a fail-closed current marker before any suite preflight can fail. */
    bool BeginRegressionSuiteReport(
        FString& OutReportPath,
        FString& OutError) const;

    /** Replaces the current marker with a terminal failed report. */
    bool WriteRegressionSuiteFailureReport(
        const TArray<FDiscTrajectorySummary>& Summaries,
        const FString& Failure,
        FString& OutReportPath,
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

    /** Applies the authoritative gameplay command contract to a parsed preset. */
    static bool IsRegressionPresetCommandValid(const FPhysicsRegressionPreset& Preset);

    /** Produces a reportable failed scenario when the gameplay launch boundary rejects a preset. */
    static FDiscTrajectorySummary MakeRegressionLaunchFailureSummary(
        const FPhysicsRegressionPreset& Preset,
        EDGHandedness Handedness,
        const FString& Failure);

    static bool EvaluateRegression(
        const FPhysicsRegressionPreset& Preset,
        FDiscTrajectorySummary& InOutSummary);

    /** Validates the exact canonical six-scenario suite and frame-rate agreement contract. */
    static bool EvaluateRegressionSuite(
        const TArray<FDiscTrajectorySummary>& Summaries,
        TArray<FString>& OutFailures);

    /** Adds evidence-source and presentation-isolation gates to canonical suite acceptance. */
    static bool EvaluateRegressionReportAcceptance(
        const TArray<FDiscTrajectorySummary>& Summaries,
        bool bAuthoritativePresetsLoaded,
        bool bPresentationTraceUnchanged,
        TArray<FString>& OutFailures);

    /** SHA-1 is computed over the exact raw bytes, including whitespace and line endings. */
    static FString ComputeRegressionPresetFileSha1(const TArray<uint8>& RawBytes);
    static bool IsRegressionPresetDigestAuthoritative(const FString& Digest);

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
    bool bRegressionPresetsAuthoritative = false;
    FString RegressionPresetFileSha1;

    // The deferred payload owns value copies so the settled disc can be destroyed
    // without invalidating a later export.
    bool bCaptureExportDeferralArmed = false;
    bool bHasPendingDeferredCaptureExport = false;
    FResolvedDiscDefinition PendingDeferredDisc;
    FThrowRelease PendingDeferredRelease;
    TArray<FDiscTrajectorySample> PendingDeferredSamples;
    TArray<FDiscGroundTransition> PendingDeferredTransitions;
    FDiscFlightTelemetry PendingDeferredTelemetry;
    FDiscTrajectorySummary PendingDeferredSummary;

    bool LoadRegressionPresets(FString& OutError);
    void BuildFallbackPresets();
    bool WriteCaptureExport(
        const FResolvedDiscDefinition& Disc,
        const FThrowRelease& Release,
        const TArray<FDiscTrajectorySample>& Samples,
        const TArray<FDiscGroundTransition>& Transitions,
        const FDiscFlightTelemetry& Telemetry,
        const FDiscTrajectorySummary& Summary,
        FString& OutError);
    bool WriteRegressionSuiteStateReport(
        const TArray<FDiscTrajectorySummary>& Summaries,
        const FString& RunState,
        const FString& Failure,
        bool bWriteArchive,
        FString& OutReportPath,
        FString& OutError) const;
    void ResetDeferredCapturePayload();
};
