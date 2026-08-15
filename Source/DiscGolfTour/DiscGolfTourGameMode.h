#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DiscGolfTypes.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfCoursePresentationDefinition.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfPresentationAudio.h"
#include "DiscGolfPerformanceBudget.h"
#include "DiscGolfRouteTelemetry.h"
#include "DiscGolfPlayerExperience.h"
#include "DiscTrajectoryTypes.h"
#include "DiscGolfTourGameMode.generated.h"

class ADiscActor;
class ADiscGolfHoleActor;
class ADevCourseBootstrap;
class AWindDirector;
class UDiscTrajectorySubsystem;
class ADiscReplayActor;
class ADiscBroadcastCameraDirector;
class ADiscGolfFlyoverRouteActor;
class ADiscGolfLevelDesignReviewActor;
class ADiscGolfFixtureQaRunner;

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

    UFUNCTION(BlueprintCallable) void RequestThrow(const FThrowCommand& Command);
    UFUNCTION(BlueprintCallable) void ResetHole();
    UFUNCTION(BlueprintCallable) void CyclePhysicsRegressionPreset();
    UFUNCTION(BlueprintCallable) void RunSelectedPhysicsRegression();
    UFUNCTION(BlueprintCallable) void RunPhysicsRegressionSuite();
    UFUNCTION(BlueprintCallable) void ToggleShotTracer();
    UFUNCTION(BlueprintCallable) void ToggleInstantReplay();
    UFUNCTION(BlueprintCallable) void CycleReplayCamera();
    UFUNCTION(BlueprintCallable) void RunCourseRulesFixture(const FString& FixtureName);
    UFUNCTION(BlueprintCallable) bool LoadCourse(const FString& CourseName);
    UFUNCTION(BlueprintCallable) void ToggleCourse();
    UFUNCTION(BlueprintCallable) void PreviewCourseFlyover();
    UFUNCTION(BlueprintCallable) void AdvanceToNextHole();
    UFUNCTION(BlueprintCallable) void RestartRound();
    UFUNCTION(BlueprintCallable) void ToggleScorecard();
    UFUNCTION(BlueprintCallable) void ToggleDeveloperHud();
    UFUNCTION(BlueprintCallable) void SkipCurrentPresentation();
    UFUNCTION(BlueprintCallable) void ToggleLevelDesignReview();
    UFUNCTION(BlueprintCallable) bool LoadRoundHole(int32 HoleNumber);
    UFUNCTION(BlueprintCallable) bool StartNeedleGateRouteTelemetry(bool bResetExisting = false);
    UFUNCTION(BlueprintCallable) bool SelectRouteTelemetry(const FString& RouteId);
    UFUNCTION(BlueprintCallable) bool SetRouteTelemetryTradeoffUnderstood(int32 Understood);
    UFUNCTION(BlueprintCallable) bool SetRouteTelemetryNextShotClear(int32 Clear);
    UFUNCTION(BlueprintCallable) bool SaveRouteTelemetry();
    UFUNCTION(BlueprintCallable) void StopRouteTelemetry();
    UFUNCTION(Exec) void DGT_RunSelectedRegression() { RunSelectedPhysicsRegression(); }
    UFUNCTION(Exec) void DGT_RunRegressionSuite() { RunPhysicsRegressionSuite(); }
    UFUNCTION(Exec) void DGT_ToggleTracer() { ToggleShotTracer(); }
    UFUNCTION(Exec) void DGT_Replay() { ToggleInstantReplay(); }
    UFUNCTION(Exec) void DGT_RunRulesFixture(const FString& FixtureName) { RunCourseRulesFixture(FixtureName); }
    UFUNCTION(Exec) void DGT_LoadCourse(const FString& CourseName) { LoadCourse(CourseName); }
    UFUNCTION(Exec) void DGT_ToggleCourse() { ToggleCourse(); }
    UFUNCTION(Exec) void DGT_PreviewFlyover() { PreviewCourseFlyover(); }
    UFUNCTION(Exec) void DGT_NextHole() { AdvanceToNextHole(); }
    UFUNCTION(Exec) void DGT_RestartRound() { RestartRound(); }
    UFUNCTION(Exec) void DGT_Scorecard() { ToggleScorecard(); }
    UFUNCTION(Exec) void DGT_ToggleDeveloperHud() { ToggleDeveloperHud(); }
    UFUNCTION(Exec) void DGT_SkipPresentation() { SkipCurrentPresentation(); }
    UFUNCTION(Exec) void DGT_ToggleLevelDesignReview() { ToggleLevelDesignReview(); }
    UFUNCTION(Exec) void DGT_LoadHole(int32 HoleNumber) { LoadRoundHole(HoleNumber); }
    UFUNCTION(Exec) void DGT_StartNeedleGateTelemetry() { StartNeedleGateRouteTelemetry(false); }
    UFUNCTION(Exec) void DGT_ResetNeedleGateTelemetry() { StartNeedleGateRouteTelemetry(true); }
    UFUNCTION(Exec) void DGT_SelectTelemetryRoute(const FString& RouteId) { SelectRouteTelemetry(RouteId); }
    UFUNCTION(Exec) void DGT_TelemetryTradeoffUnderstood(int32 Understood) { SetRouteTelemetryTradeoffUnderstood(Understood); }
    UFUNCTION(Exec) void DGT_TelemetryNextShotClear(int32 Clear) { SetRouteTelemetryNextShotClear(Clear); }
    UFUNCTION(Exec) void DGT_SaveRouteTelemetry() { SaveRouteTelemetry(); }
    UFUNCTION(Exec) void DGT_StopRouteTelemetry() { StopRouteTelemetry(); }
    UFUNCTION(BlueprintPure) bool CanPlayerThrow() const;

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
    UFUNCTION(BlueprintPure) bool IsLevelDesignReviewVisible() const;
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
    UFUNCTION(BlueprintCallable) void ResetPerformanceTelemetry();
    UFUNCTION(BlueprintCallable) bool CapturePerformanceSnapshot();
    UFUNCTION(Exec) void DGT_ResetPerformanceTelemetry() { ResetPerformanceTelemetry(); }
    UFUNCTION(Exec) void DGT_CapturePerformance() { CapturePerformanceSnapshot(); }
    EDiscGolfPerformanceBudgetState GetPerformanceBudgetState() const { return PerformanceBudgetState; }
    UFUNCTION(BlueprintPure) bool IsCourseFlyoverActive() const;
    UFUNCTION(BlueprintPure) bool HasLastTrajectorySummary() const;
    FDiscTrajectorySummary GetLastTrajectorySummary() const;

private:
    UPROPERTY() TObjectPtr<ADiscActor> ActiveDisc;
    UPROPERTY() TObjectPtr<ADiscGolfHoleActor> ActiveHole;
    UPROPERTY() TObjectPtr<ADevCourseBootstrap> DevBootstrap;
    UPROPERTY() TObjectPtr<AWindDirector> WindDirector;
    UPROPERTY() TObjectPtr<ADiscReplayActor> ReplayActor;
    UPROPERTY() TObjectPtr<ADiscBroadcastCameraDirector> BroadcastCameraDirector;
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
    bool bLevelDesignReviewRequested = false;
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
    int32 RegressionPresentationAudioTraceBaseline = 0;
    TArray<int32> RegressionQueue;
    TArray<FDiscTrajectorySummary> RegressionSuiteSummaries;
    FVector SavedBaseWindMps = FVector::ZeroVector;
    float SavedGustAmplitudeMps = 0.0f;
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
    bool bHole1FlightRouteSmokeTestActive = false;
    bool bPineRidgeSmokeSawAuthoredCamera = false;
    bool bPineRidgeSmokeEnteredWindZone = false;
    int32 Hole1FlightRouteIndex = 0;
    TArray<FDiscGolfHole1FlightRouteResult> Hole1FlightRouteResults;
    bool bThreeHoleRoundSmokeTestActive = false;
    bool bScorecardVisible = false;
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
    bool TraceCourseSurfaceAtLocation(
        const FVector& LocationCm,
        ECourseSurfaceType& OutSurface,
        FVector& OutGroundLocationCm) const;
    FName ResolvePresentationSurfaceMaterialAtLocation(const FVector& LocationCm) const;
    FVector FindLastInBoundsRelief(
        const TArray<FDiscTrajectorySample>& Samples,
        const FVector& FallbackLocationCm) const;
    void SavePracticeRoundSnapshot();
    bool RestorePracticeRoundSnapshot();
    void LaunchThrow(const FThrowCommand& Command);
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
    void StartHole1FlightRouteSmokeTest();
    void StartNextHole1FlightRouteThrow();
    void RecordHole1FlightRouteResult(bool bHoledOut);
    void FinishHole1FlightRouteSmokeTest();
    void StartThreeHoleRoundSmokeTest();
    void StartRoundSmokePutt();
    void AdvanceThreeHoleRoundSmokeTest();
    void FinishThreeHoleRoundSmokeTest(bool bPassed, const FString& Reason);
    void SpawnFixturePresentationGallery();
    void CaptureVisualQAScreenshot();
    void PositionGalleryLakeWaterVisualQACamera();
    void PositionGroundCoverVisualQACamera();
    void PositionShorelineDressingVisualQACamera();
    void PositionTrailWearVisualQACamera();
    void FinishVisualQAScreenshot();
    void RefreshPerformanceTelemetry();
    void ApplyOmenPerformanceCaptureProfile();
    void StartPerformanceCaptureSamples();
    void FinishPerformanceCapture();
    const FDiscGolfHoleBlockoutDefinition* GetActiveHoleDefinition() const;
    void RefreshRouteTelemetryStatus();
    void CaptureRouteTelemetryAttempt(ADiscActor* Disc, bool bHoledOut);
    void FinalizeRouteTelemetryHoleScore();
    bool HasRouteTelemetryBasketVisibility(const FVector& FromLocationCm, const ADiscActor* Disc) const;
    void FinishRouteTelemetrySmokeTest();

    UFUNCTION() void HandleDiscSettled(ADiscActor* Disc, FVector FinalLocation);
    UFUNCTION() void HandleDiscHoledOut(ADiscActor* Disc);
    UFUNCTION() void HandleReplayFinished(ADiscReplayActor* FinishedReplay);
    UFUNCTION() void HandleFlyoverFinished(ADiscGolfFlyoverRouteActor* FinishedRoute);
};
