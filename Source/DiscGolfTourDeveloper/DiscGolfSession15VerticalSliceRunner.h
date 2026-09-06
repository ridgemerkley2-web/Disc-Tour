#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfTypes.h"
#include "DiscGolfSession15VerticalSliceRunner.generated.h"

class ADiscActor;
class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class ADiscGolfTourPlayerController;
class FJsonObject;
class UDiscBagComponent;
class UDiscGolfTourGameInstance;

/**
 * Four-process, external-UserDir acceptance runner for the Session 15 one-hole
 * vertical slice. It observes and drives the existing player/GameMode seams;
 * it never owns a lie, score, release, flight, replay, or save authority.
 */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession15VerticalSliceRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession15VerticalSliceRunner();
    virtual void Tick(float DeltaSeconds) override;
    void Start();

private:
    enum class EPhase : uint8
    {
        Invalid,
        Setup,
        Drive,
        Finish,
        Verify
    };

    enum class EStage : uint8
    {
        Idle,
        DriveWarmup,
        AwaitingDriveRelease,
        AwaitingDriveOutcome,
        DriveReplay,
        DrivePerformanceStabilize,
        DrivePerformanceCapture,
        AwaitingCircle2Outcome,
        AwaitingCircle1Ready,
        AwaitingCircle1Outcome,
        FinishReplay,
        Finished
    };

    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolfTourPlayerController> PlayerController;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> Golfer;
    UPROPERTY() TObjectPtr<UDiscBagComponent> Bag;
    UPROPERTY() TObjectPtr<UDiscGolfTourGameInstance> TourGameInstance;
    UPROPERTY() TObjectPtr<ADiscActor> ObservedDisc;

    EPhase Phase = EPhase::Invalid;
    EStage Stage = EStage::Idle;
    FString AcceptedUserDir;
    FString FailureReason;
    FString SetupCharacterJson;
    FString SetupOutfitJson;
    FString SetupSettingsJson;
    FString SetupDiscJson;
    FString DriveRoundJson;
    FString DriveLieJson;
    FString DriveDiscJson;
    double StageStartedSeconds = 0.0;
    double ReplayStartedSeconds = 0.0;
    int32 BaselineReleaseCount = 0;
    int32 BaselineStrokes = 0;
    int32 BaselineAudioCount = 0;
    int32 BaselineThrowLabCount = 0;
    int32 FlightSampleCount = 0;
    int32 AudioEventDelta = 0;
    int32 ThrowLabCount = 0;
    int32 PerformanceWidth = 0;
    int32 PerformanceHeight = 0;
    int32 PerformanceSampleCount = 0;
    int32 PerformanceHitchCount = 0;
    uint64 PerformanceMemoryBytes = 0;
    float DriveP95FrameMs = 0.0f;
    FString PerformanceRHI;
    FString PerformanceRHIDetail;
    FGuid TouchInstanceId;
    bool bCollectPerformance = false;
    bool bCameraSeen = false;
    bool bWindSeen = false;
    bool bDiscIdentitySeen = false;
    bool bReplayExercised = false;
    bool bThrowLabReplayExercised = false;
    bool bNaturalCircle2Lie = false;
    bool bCircle2SettledToCircle1 = false;
    bool bCaughtHoleOut = false;
    bool bScreenshotRequested = false;
    bool bFinished = false;
    TArray<float> PerformanceFrameMs;

    bool ResolveInvocation(FString& OutError);
    bool ResolveRuntime(FString& OutError);
    bool ValidateVerifiedMetaHuman(FString& OutError) const;
    bool ValidateCommonAgainstSetup(FString& OutError, bool bRequireSetupEquipment = true);
    bool ValidateFreshHoleOne(FString& OutError) const;
    bool RunSetup(FString& OutError);
    bool BeginDrive(FString& OutError);
    bool BeginFinish(FString& OutError);
    bool RunVerify(FString& OutError);
    void ObserveDriveRelease();
    void BeginDriveReplay();
    void FinishDriveReplay();
    void CompleteDrivePerformanceCapture();
    bool LaunchCircle2(FString& OutError);
    bool LaunchCircle1(FString& OutError);
    void BeginFinishReplay();
    void FinishFinishReplay();
    bool BindObservedDisc(FString& OutError);
    bool CaptureScreenshot(const FString& PhaseName, const FString& FileName);
    bool WritePhaseReport(bool bPassed, FString& OutPath) const;
    bool WriteDrivePerformanceReport(FString& OutPath) const;
    bool WriteCanonicalReport(FString& OutPath) const;
    bool LoadPhaseReport(const FString& PhaseName, TSharedPtr<FJsonObject>& OutReport, FString& OutError) const;
    FString GetReportRoot() const;
    FString GetPhaseReportPath(const FString& PhaseName) const;
    FString GetPhaseName() const;
    float ComputeP95FrameMs() const;
    void Pass();
    void Fail(const FString& Reason);
    void SetStage(EStage NewStage);
    double SecondsInStage() const;

    UFUNCTION() void HandleObservedDiscSettled(ADiscActor* Disc, FVector FinalLocation);
    UFUNCTION() void HandleObservedDiscHoledOut(ADiscActor* Disc);
};
