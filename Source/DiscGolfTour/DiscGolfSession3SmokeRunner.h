#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession3SmokeRunner.generated.h"

class ADiscActor;
class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class UDiscGolfRHBHThrowAdapterComponent;
class UDiscGolfThrowComponent;
class UThrowControllerComponent;

/**
 * Timer-driven, command-line Session 3 acceptance runner.
 *
 * The game mode only needs to spawn this actor and call Start() for
 * -Session3OneThrowSmokeTest. The runner uses the real throw controller to
 * author commands and the Pawn's normal montage/adapter seam to reach the
 * existing authoritative launch/flight path.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfSession3SmokeRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession3SmokeRunner();
    virtual void Start();

protected:
    enum class EStage : uint8
    {
        WaitingForGameplay,
        BuildingCancellationCommand,
        AwaitingCancellationPose,
        ObservingCancelledAttempt,
        BuildingLiveCommand,
        AwaitingRelease,
        AwaitingAnimationRecovery,
        AwaitingGameplayRecovery,
        Finished
    };

    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> Golfer;
    UPROPERTY() TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> ThrowAdapter;
    UPROPERTY() TObjectPtr<UDiscGolfThrowComponent> FrameworkThrowComponent;
    UPROPERTY() TObjectPtr<ADiscActor> LaunchedDisc;

    FThrowCommand LiveCommand;
    FTimerHandle PollTimer;
    EStage Stage = EStage::WaitingForGameplay;
    double StageStartWorldSeconds = 0.0;
    int32 BaselineWorldDiscCount = 0;
    int32 BaselineTotalReleaseCount = 0;
    int32 BaselineStrokes = 0;
    int64 CancellationAttemptSerial = 0;
    int64 LiveAttemptSerial = 0;
    int32 CancellationRecoveryCallbackCount = 0;
    int32 LiveRecoveryCallbackCount = 0;
    bool bSawAimAvailable = false;
    bool bSawAnimationActive = false;
    bool bSawCancellationPose = false;
    bool bSawRelease = false;
    bool bSawNonZeroMotion = false;
    bool bSawAuthoritativeFlight = false;
    bool bSawPostReleaseAnimationActive = false;
    int32 FollowThroughPhaseEventCount = 0;
    int32 RecoveryPhaseEventCount = 0;
    bool bSawAnimationRecovery = false;
    bool bSawCameraReturn = false;
    bool bSawNextActionBegin = false;
    bool bFinished = false;

    void Poll();
    bool BeginTimingCapture(bool bForceRHBHDrive = true);
    bool FinishTimingCapture(FThrowCommand& OutCommand);
    void RunCancellationTrial();
    void ObserveCancellationPose();
    void ObserveCancelledAttempt();
    void BeginLiveThrow();
    void ObserveRelease();
    void ObserveAnimationRecovery();
    void ObserveGameplayRecovery();
    int32 CountWorldDiscs() const;
    int32 CountWorldGolfers() const;
    void SetStage(EStage NewStage);
    double SecondsInStage() const;
    virtual void Fail(const FString& Reason);
    virtual void Pass();

    UFUNCTION()
    void HandleThrowRecovered(int64 AttemptSerial, bool bDiscWasReleased);

    UFUNCTION()
    void HandleThrowPhaseChanged(EDGThrowPhase NewPhase);
};
