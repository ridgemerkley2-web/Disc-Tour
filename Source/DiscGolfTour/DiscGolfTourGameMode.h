#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DiscGolfTypes.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfCoursePresentationDefinition.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfPresentationAudio.h"
#include "DiscGolfCameraViewContract.h"
#include "DiscGolfPerformanceBudget.h"
#include "DiscGolfRouteTelemetry.h"
#include "DiscGolfPlayerExperience.h"
#include "DiscTrajectoryTypes.h"
#include "DiscGolfPlayabilityTypes.h"
#include "DiscGolfRuntimeCheckpointJournal.h"
#include "Serialization/Archive.h"
#include "DiscGolfTourGameMode.generated.h"

class ADiscActor;
class ADiscGolfHoleActor;
class ADevCourseBootstrap;
class AWindDirector;
class UDiscTrajectorySubsystem;
class ADiscReplayActor;
class ADiscBroadcastCameraDirector;
class ADiscGolfFlyoverRouteActor;
class ADiscGolfFixtureQaRunner;
class ADiscGolfSession15VerticalSliceRunner;
class UDiscGolfPresentationAudioRouterComponent;
class UDiscGolfPlayabilityMonitorComponent;
struct FDGDiscInstance;

/**
 * Result from the pure Shipping performance-capture launch validator.
 * A normal launch is not an attempt and remains unaffected. An attempted
 * launch is accepted only when every release-capture isolation guard passes.
 */
struct DISCGOLFTOUR_API FDiscGolfReleasePerformanceCaptureLaunchValidation
{
    bool bAttempted = false;
    bool bAccepted = false;
    int32 HoleNumber = 0;
    float DurationSeconds = 0.0f;
    FString UserDirToken;
    FString Error;
};

USTRUCT()
struct FDiscGolfHole1FlightRouteResult
{
    GENERATED_BODY()

    UPROPERTY() FString Scenario;
    UPROPERTY() FString RouteId;
    UPROPERTY() EThrowStyle ThrowStyle = EThrowStyle::Backhand;
    UPROPERTY() float FinalCarryMeters = 0.0f;
    UPROPERTY() float LateralMeters = 0.0f;
    UPROPERTY() float RemainingToBasketMeters = 0.0f;
    UPROPERTY() int32 SampleCount = 0;
    UPROPERTY() int32 FixtureContactCount = 0;
    UPROPERTY() bool bPassed = false;
    UPROPERTY() FString Reason;
};

UCLASS()
class DISCGOLFTOUR_API ADiscGolfTourGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADiscGolfTourGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable) bool RequestThrow(const FThrowCommand& Command);
    /**
     * C++-only animation handoff for the local RHBH presentation. It accepts only
     * the player adapter's already-committed transaction. Only the release location
     * comes from the character rig; FThrowCommand and the existing launch solver
     * remain the sole gameplay authority.
     */
    bool RequestThrowFromGrip(
        const FThrowCommand& Command,
        const FTransform& GripWorldTransform);
    UFUNCTION(BlueprintCallable) void ResetHole();
    void CyclePhysicsRegressionPreset();
    void RunSelectedPhysicsRegression();
    void RunPhysicsRegressionSuite();
    /** Pure seams used by regression-state tests and the runtime fail-closed gates. */
    static bool IsPresentationSequenceUnchanged(uint64 Baseline, uint64 Current)
    {
        return Baseline == Current;
    }
    static bool ShouldCancelRegressionForReset(bool bIsRegressionActive, bool bIsInternalReset)
    {
        return bIsRegressionActive && !bIsInternalReset;
    }
    /** Pure player-ingress provenance seam shared by runtime and focused tests. */
    static bool IsPlayerThrowProvenanceValid(
        const FThrowCommand& Command,
        EDGHandedness ActiveProfileHandedness,
        EDiscShotContext AuthoritativeShotContext,
        const FDGDiscInstance& SelectedInstance);
    /**
     * Pure pre-spawn seam shared by LaunchThrow and focused ingress tests.
     * The animated grip must remain near the authoritative pre-animation lie,
     * and the release snapshot is unchanged unless every override check passes.
     */
    static constexpr float MaximumAnimatedGripOriginDistanceCm = 300.0f;
    static bool TryPrepareReleaseForOriginOverride(
        const FVector* ReleaseLocationOverrideCm,
        const FVector& AimLineOriginCm,
        float AimReferenceDistanceCm,
        FThrowRelease& InOutRelease,
        FString& OutError);
    /**
     * Pure fail-closed seam for the narrow Shipping performance harness.
     * The caller supplies filesystem facts observed before claiming the
     * capture namespace so automation tests can cover every rejection path.
     */
    static FDiscGolfReleasePerformanceCaptureLaunchValidation
        ValidateReleasePerformanceCaptureLaunch(
            const FString& CommandLine,
            bool bIsUnattended,
            const FString& ResolvedUserDir,
            const FString& ProjectDirectory,
            const FString& RootDirectory,
            bool bCaptureDirectoryAlreadyExists,
            bool bSaveGameDirectoryAlreadyExists);
    /**
     * Pure authority seam shared by the public Blueprint wrappers and tests.
     * Shipping capture mutation is private to the guarded native sequence;
     * Development retains the existing callable telemetry workflow.
     */
    static bool IsPublicPerformanceCaptureMutationAllowed(bool bDevelopmentContent)
    {
        return bDevelopmentContent;
    }
    UFUNCTION(BlueprintCallable) void ToggleShotTracer();
    UFUNCTION(BlueprintCallable) void ToggleInstantReplay();
    UFUNCTION(BlueprintCallable) void CycleReplayCamera();
    UFUNCTION(BlueprintCallable) void ToggleReplayPause();
    UFUNCTION(BlueprintCallable) void SeekReplayRelative(float DeltaSeconds);
    UFUNCTION(BlueprintCallable) void CycleReplayPlaybackRate();
    UFUNCTION(BlueprintCallable) void RunCourseRulesFixture(const FString& FixtureName);
    UFUNCTION(BlueprintCallable) bool LoadCourse(const FString& CourseName);
    UFUNCTION(BlueprintCallable) void ToggleCourse();
    UFUNCTION(BlueprintCallable) void PreviewCourseFlyover();
    UFUNCTION(BlueprintCallable) void AdvanceToNextHole();
    UFUNCTION(BlueprintCallable) void RestartRound();
    UFUNCTION(BlueprintCallable) void ToggleScorecard();
    UFUNCTION(BlueprintCallable) void ToggleDeveloperHud();
    UFUNCTION(BlueprintCallable) void ToggleSelectedDiscFavorite();
    UFUNCTION(BlueprintCallable) void SkipCurrentPresentation();
    UFUNCTION(BlueprintCallable) bool LoadRoundHole(int32 HoleNumber);
    bool StartNeedleGateRouteTelemetry(bool bResetExisting = false);
    bool SelectRouteTelemetry(const FString& RouteId);
    bool SetRouteTelemetryTradeoffUnderstood(int32 Understood);
    bool SetRouteTelemetryNextShotClear(int32 Clear);
    bool SaveRouteTelemetry();
    void StopRouteTelemetry();
    /** True while the native boot front end owns local input. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Front End")
    bool IsMainMenuVisible() const { return bMainMenuVisible; }
    /**
     * Starts a fresh Pine Ridge round or continues the currently bootstrapped
     * Pine Ridge round, then returns input and camera ownership to gameplay.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Front End")
    bool StartOrContinueFromMainMenu();
    /**
     * Returns an idle round to the native front end without replacing the
     * existing round, scoring, save, or gameplay authority.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Front End")
    bool ReturnToMainMenu();
    UFUNCTION(Exec) void DGT_Replay() { ToggleInstantReplay(); }
    UFUNCTION(Exec) void DGT_ReplayPause() { ToggleReplayPause(); }
    UFUNCTION(Exec) void DGT_ReplaySeek(float DeltaSeconds) { SeekReplayRelative(DeltaSeconds); }
    UFUNCTION(Exec) void DGT_ReplayRate() { CycleReplayPlaybackRate(); }
    UFUNCTION(Exec) void DGT_PreviewFlyover() { PreviewCourseFlyover(); }
    UFUNCTION(Exec) void DGT_NextHole() { AdvanceToNextHole(); }
    UFUNCTION(Exec) void DGT_RestartRound() { RestartRound(); }
    UFUNCTION(Exec) void DGT_Scorecard() { ToggleScorecard(); }
    UFUNCTION(Exec) void DGT_ToggleDiscFavorite() { ToggleSelectedDiscFavorite(); }
    UFUNCTION(Exec) void DGT_SkipPresentation() { SkipCurrentPresentation(); }
    UFUNCTION(BlueprintPure) bool CanPlayerThrow() const;
    /**
     * True only while the local player can safely pause gameplay and edit the
     * presentation-only character profile. This deliberately excludes every
     * active throw, camera, replay, and lie-transition authority.
     */
    UFUNCTION(BlueprintPure) bool CanOpenCharacterCreator() const;

    UFUNCTION(BlueprintPure) int32 GetStrokes() const { return Strokes; }
    UFUNCTION(BlueprintPure) int32 GetPenaltyStrokes() const { return PenaltyStrokes; }
    UFUNCTION(BlueprintPure) bool IsHoleComplete() const { return bHoleComplete; }
    UFUNCTION(BlueprintPure) ELieType GetCurrentLieType() const { return CurrentLieType; }
    UFUNCTION(BlueprintPure) FDiscGolfLieState GetCurrentLieState() const { return CurrentLieState; }
    UFUNCTION(BlueprintPure) FString GetLieRulesStatusText() const { return LieRulesStatusText; }
    UFUNCTION(BlueprintPure) EDiscShotContext GetCurrentShotContext() const;
    UFUNCTION(BlueprintPure) float GetBasketDistanceMeters() const;
    UFUNCTION(BlueprintPure) float GetAimErrorAtBasketCm() const;
    UFUNCTION(BlueprintPure) ADiscActor* GetActiveDisc() const { return ActiveDisc; }
    UFUNCTION(BlueprintPure) ADiscGolfHoleActor* GetActiveHole() const { return ActiveHole; }
    UFUNCTION(BlueprintPure) AWindDirector* GetWindDirector() const { return WindDirector; }
    /**
     * HUD wind shares authoritative semantics: latest recorded solver sample
     * during flight, otherwise the deterministic phase of the next accepted shot.
     */
    bool TryGetAuthoritativeHudWindSample(
        const FVector& PreThrowSampleLocationCm,
        FVector& OutSampleLocationCm,
        FVector& OutWindMps,
        FString& OutError) const;
    /** Pure pre-spawn seam shared by throw authority, HUD preview, and automation. */
    static bool TrySampleWindForAcceptedShot(
        const AWindDirector* InWindDirector,
        int32 AcceptedShotSequence,
        const FVector& SampleLocationCm,
        float& OutPhaseOriginSeconds,
        FVector& OutWindMps,
        FString& OutError);
    /**
     * Resolves the active-hole semantic surface together with the continuous
     * visible ground contact at an XY location. Terrain flight physics uses
     * this same query so its friction telemetry matches final lie rules.
     */
    bool TraceCourseSurfaceAtLocation(
        const FVector& LocationCm,
        ECourseSurfaceType& OutSurface,
        FVector& OutGroundLocationCm) const;
    UFUNCTION(BlueprintPure) bool HasLastRelease() const { return bHasLastRelease; }
    UFUNCTION(BlueprintPure) FThrowRelease GetLastRelease() const { return LastRelease; }
    UFUNCTION(BlueprintPure) float GetLastReleaseAgeSeconds() const;
    UFUNCTION(BlueprintPure) bool HasLastFlightTelemetry() const { return bHasLastFlightTelemetry; }
    UFUNCTION(BlueprintPure) FDiscFlightTelemetry GetLastFlightTelemetry() const { return LastFlightTelemetry; }
    UFUNCTION(BlueprintPure) FString GetTrajectoryStatusText() const { return TrajectoryStatusText; }
    UFUNCTION(BlueprintPure) FString GetSelectedRegressionPresetName() const;
    UFUNCTION(BlueprintPure) bool IsPhysicsRegressionRunning() const { return bRegressionActive; }
    UFUNCTION(BlueprintPure) bool IsShotTracerEnabled() const { return bShotTracerEnabled; }
    UFUNCTION(BlueprintPure) bool HasReplayCapture() const { return LastReplaySamples.Num() >= 2; }
    UFUNCTION(BlueprintPure) bool IsInstantReplayActive() const { return ReplayActor != nullptr; }
    UFUNCTION(BlueprintPure) float GetReplayProgress01() const;
    UFUNCTION(BlueprintPure) float GetReplayDurationSeconds() const;
    UFUNCTION(BlueprintPure) float GetReplayPlaybackRate() const;
    UFUNCTION(BlueprintPure) FString GetReplayCameraLabel() const;
    UFUNCTION(BlueprintPure) FString GetPresentationStatusText() const;
    UFUNCTION(BlueprintPure) bool IsBroadcastCameraActive() const { return BroadcastCameraDirector != nullptr; }
    UFUNCTION(BlueprintPure) FString GetBroadcastCameraStatusText() const;
    UFUNCTION(BlueprintPure) FString GetCourseStatusText() const { return CourseStatusText; }
    UFUNCTION(BlueprintPure) FDiscGolfRoundState GetRoundState() const { return RoundState; }
    UFUNCTION(BlueprintPure) bool HasAuthoredRound() const { return !RoundState.HoleScores.IsEmpty(); }
    UFUNCTION(BlueprintPure) bool IsRoundComplete() const { return RoundState.bRoundComplete; }
    UFUNCTION(BlueprintPure) bool IsScorecardVisible() const { return bScorecardVisible; }
    UFUNCTION(BlueprintPure) bool IsDeveloperHudVisible() const { return bDeveloperHudVisible; }
    UFUNCTION(BlueprintPure) bool IsHoleIntroVisible() const;
    UFUNCTION(BlueprintPure) float GetHoleIntroProgress01() const;
    UFUNCTION(BlueprintPure) FDiscGolfShotPresentationResult GetLastShotPresentationResult() const { return LastShotPresentationResult; }
    UFUNCTION(BlueprintPure) float GetLastShotResultAgeSeconds() const;
    UFUNCTION(BlueprintPure) FString GetHoleCompletionLabel() const;
    UFUNCTION(BlueprintPure) FDiscGolfPlayerSettings GetPlayerSettings() const;
    UFUNCTION(BlueprintPure) bool IsBasketDirectlyVisible() const;
    UFUNCTION(BlueprintPure) bool IsRouteTelemetryActive() const { return bRouteTelemetryActive; }
    UFUNCTION(BlueprintPure) FString GetRouteTelemetryRouteLabel() const { return RouteTelemetryRouteLabel; }
    UFUNCTION(BlueprintPure) FString GetRouteTelemetryProgressText() const { return RouteTelemetryProgressText; }
    UFUNCTION(BlueprintPure) FString GetRouteTelemetryLastResultText() const { return RouteTelemetryLastResultText; }
    UFUNCTION(BlueprintPure) FString GetRouteTelemetryIntentText() const { return RouteTelemetryIntentText; }
    UFUNCTION(BlueprintPure) int32 GetRoundScoreToPar() const { return DiscGolfRound::ScoreToPar(RoundState); }
    UFUNCTION(BlueprintPure) FString GetRoundScoreLabel() const { return DiscGolfRound::ScoreLabel(GetRoundScoreToPar()); }
    UFUNCTION(BlueprintPure) FString GetPresentationAudioStatusText() const;
    UFUNCTION(BlueprintPure) int32 GetPresentationAudioTraceCount() const { return PresentationAudioEventTrace.Num(); }
    UFUNCTION(BlueprintPure) FString GetPerformanceStatusText() const { return PerformanceStatusText; }
    UFUNCTION(BlueprintPure) UDiscGolfPlayabilityMonitorComponent* GetPlayabilityMonitor() const
    {
        return PlayabilityMonitor;
    }
    UFUNCTION(BlueprintPure) FDGPlayabilityCheckResult GetLastPlayabilityFailure() const
    {
        return LastPlayabilityFailure;
    }
    UFUNCTION(BlueprintPure) int32 GetPlayabilityFailureCount() const
    {
        return PlayabilityFailureCount;
    }
    UFUNCTION(BlueprintCallable) void ResetPerformanceTelemetry();
    UFUNCTION(BlueprintCallable) bool CapturePerformanceSnapshot();
    EDiscGolfPerformanceBudgetState GetPerformanceBudgetState() const { return PerformanceBudgetState; }
    UFUNCTION(BlueprintPure) bool IsCourseFlyoverActive() const;
    UFUNCTION(BlueprintPure) bool HasLastTrajectorySummary() const;
    FDiscTrajectorySummary GetLastTrajectorySummary() const;
    bool DeferNextTrajectoryExport(FString& OutError);
    bool FlushDeferredTrajectoryExport(
        FDiscTrajectorySummary& OutSummary,
        FString& OutError);
    bool DiscardDeferredTrajectoryExport();
    bool IsTrajectoryExportDeferralArmed() const;
    bool HasPendingDeferredTrajectoryExport() const;
    bool GetPendingDeferredTrajectorySummary(FDiscTrajectorySummary& OutSummary) const;

private:
    friend class ADiscGolfSession15VerticalSliceRunner;

    UPROPERTY() TObjectPtr<ADiscActor> ActiveDisc;
    UPROPERTY() TObjectPtr<ADiscGolfHoleActor> ActiveHole;
    UPROPERTY() TObjectPtr<ADevCourseBootstrap> DevBootstrap;
    UPROPERTY() TObjectPtr<AWindDirector> WindDirector;
    UPROPERTY() TObjectPtr<ADiscReplayActor> ReplayActor;
    UPROPERTY() TObjectPtr<ADiscBroadcastCameraDirector> BroadcastCameraDirector;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfPresentationAudioRouterComponent> PresentationAudioRouter;
    /** Development alarm only. Existing round, throw, flight, lie, and recovery paths remain authoritative. */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfPlayabilityMonitorComponent> PlayabilityMonitor;
    UPROPERTY() TObjectPtr<ADiscGolfFixtureQaRunner> FixtureQaRunner;
    UPROPERTY() FDiscGolfCourseManifestDefinition ActiveCourseManifest;
    UPROPERTY() TArray<FDiscGolfHoleBlockoutDefinition> ActiveHoleDefinitions;
    UPROPERTY() FDiscGolfRoundState RoundState;
    FDiscGolfCoursePresentationDefinition ActiveCoursePresentation;

    int32 Strokes = 0;
    int32 PenaltyStrokes = 0;
    int32 LastThrowShotNumber = 0;
    bool bHoleComplete = false;
    bool bHasLastRelease = false;
    bool bDeveloperHudVisible = false;
    bool bHasLastFlightTelemetry = false;
    bool bHoleIntroActive = false;
    float HoleIntroStartWorldSeconds = -1.0f;
    float HoleIntroDurationSeconds = 3.25f;
    FDiscGolfShotPresentationResult LastShotPresentationResult;
    float LastShotResultWorldSeconds = -1.0f;
    bool bBasketDirectlyVisibleCached = false;
    float BasketVisibilityRefreshAccumulator = 0.0f;
    ELieType CurrentLieType = ELieType::Tee;
    FVector CurrentLieLocation = FVector::ZeroVector;
    FVector ThrowStartLieLocation = FVector::ZeroVector;
    FDiscGolfLieState CurrentLieState;
    FThrowRelease LastRelease;
    float LastReleaseWorldSeconds = -1.0f;
    FDiscFlightTelemetry LastFlightTelemetry;
    int32 SelectedRegressionPresetIndex = 0;
    FName ActiveRegressionPresetId = NAME_None;
    int32 ActiveRegressionRenderFps = 0;
    bool bRegressionActive = false;
    bool bRegressionSuiteActive = false;
    bool bRegressionEnvironmentSaved = false;
    int32 RegressionQueuePosition = 0;
    uint64 RegressionPresentationAudioSequenceBaseline = 0;
    bool bRegressionInternalReset = false;
    TArray<int32> RegressionQueue;
    TArray<FDiscTrajectorySummary> RegressionSuiteSummaries;
    FVector SavedBaseWindMps = FVector::ZeroVector;
    float SavedGustAmplitudeMps = 0.0f;
    float SavedGustFrequencyHz = 0.12f;
    float SavedMaxFps = 0.0f;
    FString TrajectoryStatusText = TEXT("Trajectory auto-export ready");
    FString LastRegressionReportPath;
    TArray<FDiscTrajectorySample> LastReplaySamples;
    FResolvedDiscDefinition LastReplayDisc;
    FThrowRelease LastReplayRelease;
    bool bShotTracerEnabled = false;
    bool bLieTransitionActive = false;
    bool bCourseSmokeTestActive = false;
    bool bRegressionSuiteSmokeTestActive = false;
    bool bPineRidgePlaySmokeTestActive = false;
    bool bSession12PresentationSmokeTestActive = false;
    bool bSession12BaseSmokePassed = false;
    bool bSession12ReplayControlsPassed = false;
    bool bLastReplayCaptureActualSampleProvenanceValid = false;
    bool bLastReplayCaptureNominalRateValid = false;
    float Session12ReplaySeekTargetSeconds = 0.0f;
    bool bHole1FlightRouteSmokeTestActive = false;
    bool bPineRidgeSmokeSawAuthoredCamera = false;
    bool bPineRidgeSmokeEnteredWindZone = false;
    int32 Hole1FlightRouteIndex = 0;
    TArray<FDiscGolfHole1FlightRouteResult> Hole1FlightRouteResults;
    bool bThreeHoleRoundSmokeTestActive = false;
    bool bSession16RulesSmokeTestActive = false;
    FString Session16RulesSmokeScenario;
    bool bSession16TemporaryStateBasePassed = false;
    bool bScorecardVisible = false;
    bool bMainMenuVisible = false;
    FString ReplayStatusText = TEXT("Replay waiting for a completed shot");
    FString LieRulesStatusText = TEXT("TEE - CLEAN STANCE");
    FString CourseStatusText = TEXT("COURSE INITIALIZING");
    FString CoursePresentationStatusText = TEXT("ART CONTRACT N/A");
    FString VisualQAScreenshotPath;
    FDiscGolfPresentationAudioEvent LastPresentationAudioEvent;
    TArray<FDiscGolfPresentationAudioEvent> PresentationAudioEventTrace;
    uint64 PresentationAudioEventSequence = 0;
    int32 PresentationShotSequence = 0;
    int32 LastPresentationGroundTransitionCount = 0;
    FDiscGolfPerformanceTracker PerformanceTracker;
    FDiscGolfPerformanceSummary PerformanceSummary;
    EDiscGolfPerformanceBudgetState PerformanceBudgetState = EDiscGolfPerformanceBudgetState::WarmingUp;
    float PerformanceRefreshAccumulator = 0.0f;
    FString PerformanceStatusText = TEXT("PERF WARMING 0/120");
    FString PerformanceCaptureProfile = TEXT("RuntimeCurrent");
    float PerformanceCaptureDurationSeconds = 0.0f;
    bool bPerformanceCaptureActive = false;
    bool bReleasePerformanceCapture = false;
    bool bPerformanceCaptureRuntimeContractValid = false;
    int32 PerformanceCaptureRequestedHoleNumber = 0;
    UPROPERTY() FDGPlayabilityCheckResult LastPlayabilityFailure;
    int32 PlayabilityFailureCount = 0;
    FDiscGolfRouteTelemetrySession RouteTelemetrySession;
    bool bRouteTelemetryActive = false;
    bool bRouteTelemetryShotPending = false;
    int32 PendingRouteTradeoffUnderstood = -1;
    int32 LastRouteTelemetryAttemptIndex = INDEX_NONE;
    int32 RouteTelemetryAttemptAwaitingScoreIndex = INDEX_NONE;
    FString RouteTelemetryRouteLabel = TEXT("ROUTE TELEMETRY OFF");
    FString RouteTelemetryProgressText;
    FString RouteTelemetryLastResultText = TEXT("No route attempt recorded");
    FString RouteTelemetryIntentText;
    FString LastRouteTelemetryReportPath;
    FDiscGolfCameraViewState CameraViewState;
    FDiscGolfCameraViewToken BroadcastCameraViewToken;
    FDiscGolfCameraViewToken ReplayCameraViewToken;
    FDiscGolfCameraViewToken FlyoverCameraViewToken;
    UPROPERTY() TObjectPtr<ADiscGolfFlyoverRouteActor> ActiveFlyoverRoute;

    // Opt-in Shipping evidence is process-emitted into a fresh redirected
    // UserDir. The writer stays open for the capture lifetime so no call site
    // can replace or recreate the journal after its exclusive creation.
    TUniquePtr<FArchive> RuntimeCheckpointJournalWriter;
    DiscGolfRuntimeCheckpointJournal::FCaptureBinding RuntimeCheckpointCaptureBinding;
    FString RuntimeCheckpointRoundId;
    double RuntimeCheckpointStartedSeconds = 0.0;
    int64 RuntimeCheckpointLastMonotonicMs = 0;
    int32 RuntimeCheckpointNextSequence = 1;
    int32 RuntimeCheckpointFirstLieMask = 0;
    bool bRuntimeCheckpointJournalRequested = false;
    bool bRuntimeCheckpointRoundStarted = false;
    bool bRuntimeCheckpointJournalTerminal = false;

    void EnsurePlayerPawn();
    void StartHole();
    void FinishHoleIntroduction();
    void FinishLieTransition();
    void RefreshBasketVisibility();
    bool InitializePineRidgeRound(FString& OutError);
    bool LoadRoundHoleByIndex(int32 HoleIndex, FString& OutError);
    void UpdateCourseStatus();
    void ReturnCameraToPlayer();
    void MovePlayerToLie(const FVector& DiscLocation);
    void UpdateLieType();
    void ResolveSettledLie(ADiscActor* Disc, const FVector& FinalLocation);
    FName ResolvePresentationSurfaceMaterialAtLocation(const FVector& LocationCm) const;
    FVector FindLastInBoundsRelief(
        const TArray<FDiscTrajectorySample>& Samples,
        const FVector& FallbackLocationCm) const;
    void SavePracticeRoundSnapshot();
    bool RestorePracticeRoundSnapshot();
    bool LaunchThrow(
        const FThrowCommand& Command,
        const FVector* ReleaseLocationOverrideCm = nullptr,
        bool bTrustedRegressionCatalogCommand = false);
    void RecordPresentationAudioEvent(
        const FDiscGolfPresentationAudioEvent& Event,
        const FVector& WorldLocationCm = FVector::ZeroVector,
        float EventTimeSeconds = 0.0f,
        bool bReplayPresentation = false,
        ECourseSurfaceType CourseSurface = ECourseSurfaceType::Fairway,
        EDiscGroundState GroundState = EDiscGroundState::Airborne,
        EBasketContactResult BasketResult = EBasketContactResult::None,
        EDiscGolfPenaltyType Penalty = EDiscGolfPenaltyType::None);
    void UpdatePresentationAudioEvents();
    void ApplyShotContextToGolfer();
    UDiscTrajectorySubsystem* GetTrajectorySubsystem() const;
    void CompleteDiscCapture(ADiscActor* Disc, bool bHoledOut);
    void RecordThrowHistory(ADiscActor* Disc, bool bHoledOut);
    void StartRegressionPreset(int32 PresetIndex);
    void AdvanceRegressionSuite();
    void FinishRegressionRun();
    void FailRegressionSuiteSmoke(const FString& Failure);
    void SaveRegressionEnvironment();
    void RestoreRegressionEnvironment();
    void StoreReplayCapture(ADiscActor* Disc);
    void DrawShotTracer() const;
    void StopInstantReplay(bool bReturnCamera);
    void StartBroadcastCameraForShot(ADiscActor* Disc, const FVector& ReleaseLocationCm, EDiscShotContext ShotContext);
    void StopBroadcastCamera(bool bPreserveForCameraBlend);
    void FinishCourseSmokeTest();
    void FinishGroundGrassSmokeTest();
    void FinishDenseForestSmokeTest();
    void FinishGalleryLakeWaterSmokeTest();
    void StartPineRidgePlaySmokeTest();
    void FinishPineRidgePlaySmokeTest(bool bHoledOut);
    void FinishSession12PresentationSmokeTest();
    void FinishSession16TemporaryStateSmokeTest();
    void StartSession16RulesSmokeTest();
    void FinishSession16RulesSmokeTest();
    void StartHole1FlightRouteSmokeTest();
    void StartNextHole1FlightRouteThrow();
    void RecordHole1FlightRouteResult(bool bHoledOut);
    void FinishHole1FlightRouteSmokeTest();
    void StartThreeHoleRoundSmokeTest();
    void StartRoundSmokePutt();
    void AdvanceThreeHoleRoundSmokeTest();
    void FinishThreeHoleRoundSmokeTest(bool bPassed, const FString& Reason);
    void ShowMainMenu();
    void RunSession16MainMenuSmokeTest();
    void RunSession17RoundFlowSmokeTest();
    void SpawnFixturePresentationGallery();
    void CaptureVisualQAScreenshot();
    void PositionGalleryLakeWaterVisualQACamera();
    void PositionGroundCoverVisualQACamera();
    void PositionShorelineDressingVisualQACamera();
    void PositionTrailWearVisualQACamera();
    void FinishVisualQAScreenshot();
    void RefreshPerformanceTelemetry();
    void ResetPerformanceTelemetryInternal();
    bool CapturePerformanceSnapshotInternal();
    void ApplyOmenPerformanceCaptureProfile();
    bool BeginPerformanceCaptureSequence();
    void StartPerformanceCaptureSamples();
    void FinishPerformanceCapture();
    const FDiscGolfHoleBlockoutDefinition* GetActiveHoleDefinition() const;
    void RefreshRouteTelemetryStatus();
    void CaptureRouteTelemetryAttempt(ADiscActor* Disc, bool bHoledOut);
    void FinalizeRouteTelemetryHoleScore();
    bool HasRouteTelemetryBasketVisibility(const FVector& FromLocationCm, const ADiscActor* Disc) const;
    void FinishRouteTelemetrySmokeTest();
    void InitializeRuntimeCheckpointJournal();
    void BeginRuntimeCheckpointRound();
    void RecordRuntimeCheckpointTee();
    void RecordRuntimeCheckpointFirstLie();
    void RecordRuntimeCheckpointHoleCompleted();
    void RecordRuntimeCheckpointFinalScorecard();
    bool WriteRuntimeCheckpointEvent(
        const FString& EventName,
        const TOptional<int32>& HoleNumber,
        bool bIncludeFinalScore);
    void GetRuntimeCheckpointCumulativeScore(
        int32& OutStrokes,
        int32& OutPenalties) const;
    void AbortRuntimeCheckpointJournal(const FString& Reason);

    UFUNCTION() void HandleDiscSettled(ADiscActor* Disc, FVector FinalLocation);
    UFUNCTION() void HandleDiscHoledOut(ADiscActor* Disc);
    UFUNCTION() void HandleReplayFinished(ADiscReplayActor* FinishedReplay);
    UFUNCTION() void HandleFlyoverFinished(ADiscGolfFlyoverRouteActor* FinishedRoute);
    UFUNCTION() void HandlePlayabilityFailure(FDGPlayabilityCheckResult Result);
};
