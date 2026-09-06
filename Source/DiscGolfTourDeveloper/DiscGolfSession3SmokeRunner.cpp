#include "DiscGolfSession3SmokeRunner.h"

#include "DiscActor.h"
#include "DiscBagComponent.h"
#include "DiscFlightComponent.h"
#include "DiscGolferPawn.h"
#include "DiscGolferPresentationComponent.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfProductionMotion.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfThrowComponent.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ThrowControllerComponent.h"
#include "TimerManager.h"

namespace
{
constexpr float PollIntervalSeconds = 0.02f;
constexpr double ReadinessTimeoutSeconds = 5.0;
constexpr double TimingCaptureDelaySeconds = 0.10;
constexpr double CancellationPoseTimeoutSeconds = 3.0;
// ReachBack is authored at 0.8 s and the former release is at 1.6 s. Keeping
// the cancelled attempt quiescent for another 1.05 s proves stale montage
// evaluation cannot release after cancellation.
constexpr double CancelledReleaseObservationSeconds = 1.05;
constexpr double ReleaseTimeoutSeconds = 5.0;
constexpr double AnimationRecoveryTimeoutSeconds = 5.0;
constexpr double GameplayRecoveryTimeoutSeconds = 55.0;

bool CommandsMatchExactly(const FThrowCommand& A, const FThrowCommand& B)
{
    return A.DiscInstanceId == B.DiscInstanceId
        && A.MoldId == B.MoldId
        && A.Plastic == B.Plastic
        && A.ThrowStyle == B.ThrowStyle
        && A.Handedness == B.Handedness
        && A.ShotContext == B.ShotContext
        && A.Direction == B.Direction
        && A.Power01 == B.Power01
        && A.HyzerDeg == B.HyzerDeg
        && A.NoseAngleDeg == B.NoseAngleDeg
        && A.LaunchAngleDeg == B.LaunchAngleDeg
        && A.TimingError == B.TimingError;
}
}

ADiscGolfSession3SmokeRunner::ADiscGolfSession3SmokeRunner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ADiscGolfSession3SmokeRunner::Start()
{
    if (bFinished || GetWorldTimerManager().IsTimerActive(PollTimer))
    {
        return;
    }

    bRequireV006ProductionProfileProof = FParse::Param(
        FCommandLine::Get(), TEXT("Session19V006ProductionProfileProof"));
    if (bRequireV006ProductionProfileProof)
    {
        if (!FParse::Value(
                FCommandLine::Get(), TEXT("Session4Profile="),
                ProductionProfileProofName)
            || ProductionProfileProofName.IsEmpty())
        {
            Fail(TEXT("v006 production profile proof requires an explicit Session4Profile"));
            return;
        }
        if (FCString::Strcmp(DiscGolfProductionMotion::ActiveVersion, TEXT("v6")) != 0
            || FCString::Strcmp(
                DiscGolfProductionMotion::ActiveAssetRevision, TEXT("v006")) != 0)
        {
            Fail(TEXT("v006 production profile proof was compiled against a different active motion revision"));
            return;
        }
    }

    GameMode = GetWorld() ? Cast<ADiscGolfTourGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    ThrowAdapter = Golfer ? Golfer->FindComponentByClass<UDiscGolfRHBHThrowAdapterComponent>() : nullptr;
    FrameworkThrowComponent = Golfer ? Golfer->FindComponentByClass<UDiscGolfThrowComponent>() : nullptr;
    if (!GameMode || !Golfer || !ThrowAdapter || !FrameworkThrowComponent
        || !Golfer->GetThrowController() || !Golfer->GetDiscBag()
        || !Golfer->GetPresentationComponent())
    {
        Fail(TEXT("game mode, player, Session 3 throw components, presentation, controller, or disc bag was unavailable"));
        return;
    }

    ThrowAdapter->OnThrowRecovered.AddUniqueDynamic(
        this, &ADiscGolfSession3SmokeRunner::HandleThrowRecovered);
    FrameworkThrowComponent->OnThrowPhaseChanged.AddUniqueDynamic(
        this, &ADiscGolfSession3SmokeRunner::HandleThrowPhaseChanged);

    GameMode->SkipCurrentPresentation();
    BaselineWorldDiscCount = CountWorldDiscs();
    BaselineTotalReleaseCount = ThrowAdapter->GetTotalReleaseCommitCount();
    BaselineStrokes = GameMode->GetStrokes();
    if (CountWorldGolfers() != 1)
    {
        Fail(TEXT("the one-throw fixture did not start with exactly one player pawn"));
        return;
    }
    SetStage(EStage::WaitingForGameplay);
    GetWorldTimerManager().SetTimer(
        PollTimer, this, &ADiscGolfSession3SmokeRunner::Poll, PollIntervalSeconds, true);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 3 ONE-THROW SMOKE START: awaiting a legal RHBH tee action."));
}

void ADiscGolfSession3SmokeRunner::Poll()
{
    if (bFinished)
    {
        return;
    }
    if (!GameMode || !Golfer || !ThrowAdapter)
    {
        Fail(TEXT("runtime authority disappeared during the smoke"));
        return;
    }
    if (CountWorldGolfers() != 1)
    {
        Fail(TEXT("the one-throw fixture created or lost a player pawn"));
        return;
    }
    if (CountWorldDiscs() > BaselineWorldDiscCount + 1)
    {
        Fail(TEXT("more than one gameplay disc was present during the throw"));
        return;
    }
    if (bSawCancellationPose && CancellationRecoveryCallbackCount != 1)
    {
        Fail(TEXT("cancelled attempt recovery callback cardinality changed after cancellation"));
        return;
    }
    if (LiveRecoveryCallbackCount > 1)
    {
        Fail(TEXT("live throw recovery event fired more than once"));
        return;
    }
    if (FollowThroughPhaseEventCount > 1 || RecoveryPhaseEventCount > 1)
    {
        Fail(TEXT("follow-through or recovery phase fired more than once"));
        return;
    }

    switch (Stage)
    {
        case EStage::WaitingForGameplay:
        {
            UThrowControllerComponent* Controller = Golfer->GetThrowController();
            const FVector AimDirection = Golfer->GetActorForwardVector();
            bSawAimAvailable = Controller && !AimDirection.ContainsNaN() && !AimDirection.IsNearlyZero();
            if (GameMode->CanPlayerThrow() && bSawAimAvailable)
            {
                if (!BeginTimingCapture())
                {
                    Fail(TEXT("existing throw controller did not enter its timing state"));
                    return;
                }
                SetStage(EStage::BuildingCancellationCommand);
            }
            else if (SecondsInStage() > ReadinessTimeoutSeconds)
            {
                Fail(TEXT("aim or legal throw state did not become available"));
            }
            break;
        }
        case EStage::BuildingCancellationCommand:
            if (SecondsInStage() >= TimingCaptureDelaySeconds)
            {
                RunCancellationTrial();
            }
            break;
        case EStage::AwaitingCancellationPose:
            ObserveCancellationPose();
            break;
        case EStage::ObservingCancelledAttempt:
            ObserveCancelledAttempt();
            break;
        case EStage::BuildingLiveCommand:
            if (SecondsInStage() >= TimingCaptureDelaySeconds)
            {
                BeginLiveThrow();
            }
            break;
        case EStage::AwaitingRelease:
            ObserveRelease();
            break;
        case EStage::AwaitingAnimationRecovery:
            ObserveAnimationRecovery();
            break;
        case EStage::AwaitingGameplayRecovery:
            ObserveGameplayRecovery();
            break;
        case EStage::Finished:
        default:
            break;
    }
}

bool ADiscGolfSession3SmokeRunner::BeginTimingCapture(bool bForceRHBHDrive)
{
    UThrowControllerComponent* Controller = Golfer ? Golfer->GetThrowController() : nullptr;
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    if (!Controller || !Bag || Bag->GetSelectedMoldId().IsNone())
    {
        return false;
    }

    Controller->CancelTiming();
    if (bForceRHBHDrive)
    {
        Controller->SetShotContext(EDiscShotContext::Drive, GameMode->GetBasketDistanceMeters());
        if (Controller->GetThrowStyle() != EThrowStyle::Backhand)
        {
            Controller->ToggleThrowStyle();
        }
    }

    FThrowCommand UnusedCommand;
    const bool bUnexpectedImmediateCommand = Controller->HandleThrowPress(
        Bag->GetSelectedMoldId(), Bag->GetSelectedPlastic(),
        Golfer->GetActorForwardVector(), UnusedCommand);
    if (bUnexpectedImmediateCommand || !Controller->IsTimingActive())
    {
        return false;
    }

    // Mirror the real Pawn's first input press. This keeps the legacy
    // presentation state inside the end-to-end fixture so cancellation must
    // prove that both character presentation systems recover together.
    Golfer->GetPresentationComponent()->BeginTiming(Controller->GetThrowStyle());
    return true;
}

bool ADiscGolfSession3SmokeRunner::FinishTimingCapture(FThrowCommand& OutCommand)
{
    UThrowControllerComponent* Controller = Golfer ? Golfer->GetThrowController() : nullptr;
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    const UDiscGolfCharacterProfile* Profile = Golfer
        ? Golfer->GetRuntimeCharacterProfile() : nullptr;
    if (!Controller || !Bag || !Profile || !GameMode
        || !Controller->HandleThrowPress(
            Bag->GetSelectedMoldId(), Bag->GetSelectedPlastic(),
            Golfer->GetActorForwardVector(), OutCommand))
    {
        return false;
    }

    // HandleThrowPress authors only the physical throw inputs. Mirror the real
    // Pawn input seam by snapshotting the stable player/equipment provenance
    // immediately after command capture and before the animation transaction.
    // Do not select or otherwise mutate equipment in this fixture.
    FDGDiscInstance SelectedInstance;
    if (!Bag->GetSelectedDiscInstance(SelectedInstance))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("SESSION 3 COMMAND CAPTURE FAIL: selected equipment instance was unavailable."));
        return false;
    }
    OutCommand.DiscInstanceId = SelectedInstance.InstanceId;
    OutCommand.Handedness = Profile->Handedness;

    const EDiscShotContext AuthoritativeShotContext = GameMode->GetCurrentShotContext();
    if (!ADiscGolfTourGameMode::IsPlayerThrowProvenanceValid(
            OutCommand, Profile->Handedness,
            AuthoritativeShotContext, SelectedInstance))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("SESSION 3 COMMAND CAPTURE FAIL: provenance did not match immediately after capture "
                 "(command_instance=%s selected_instance=%s command_mold=%s selected_mold=%s "
                 "command_hand=%d active_hand=%d command_context=%d authoritative_context=%d)."),
            *OutCommand.DiscInstanceId.ToString(), *SelectedInstance.InstanceId.ToString(),
            *OutCommand.MoldId.ToString(), *SelectedInstance.DiscDefinitionId.ToString(),
            static_cast<int32>(OutCommand.Handedness),
            static_cast<int32>(Profile->Handedness),
            static_cast<int32>(OutCommand.ShotContext),
            static_cast<int32>(AuthoritativeShotContext));
        return false;
    }
    return true;
}

void ADiscGolfSession3SmokeRunner::RunCancellationTrial()
{
    FThrowCommand CancellationCommand;
    if (!FinishTimingCapture(CancellationCommand))
    {
        Fail(TEXT("existing throw controller did not produce the cancellation fixture command"));
        return;
    }
    if (!Golfer->TryStartAnimatedRHBHThrow(CancellationCommand) || !ThrowAdapter->IsThrowActive())
    {
        Fail(TEXT("cancellation fixture did not enter the character animation state"));
        return;
    }

    bSawAnimationActive = true;
    CancellationAttemptSerial = ThrowAdapter->GetAttemptSerial();
    CancellationRecoveryCallbackCount = 0;
    SetStage(EStage::AwaitingCancellationPose);
}

void ADiscGolfSession3SmokeRunner::ObserveCancellationPose()
{
    if (ThrowAdapter->GetTotalReleaseCommitCount() != BaselineTotalReleaseCount
        || GameMode->GetActiveDisc() != nullptr
        || GameMode->GetStrokes() != BaselineStrokes
        || CountWorldDiscs() != BaselineWorldDiscCount)
    {
        Fail(TEXT("cancellation fixture launched before reaching the authored cancellation pose"));
        return;
    }

    if (FrameworkThrowComponent->CurrentPhase == EDGThrowPhase::ReachBack)
    {
        bSawCancellationPose = true;
        if (!Golfer->CancelAnimatedThrowBeforeRelease())
        {
            Fail(TEXT("evaluated pre-release cancellation was rejected"));
            return;
        }
        if (ThrowAdapter->IsThrowActive()
            || FrameworkThrowComponent->bThrowActive
            || FrameworkThrowComponent->CurrentPhase != EDGThrowPhase::Idle
            || ThrowAdapter->GetTotalReleaseCommitCount() != BaselineTotalReleaseCount
            || GameMode->GetActiveDisc() != nullptr
            || GameMode->GetStrokes() != BaselineStrokes
            || CountWorldDiscs() != BaselineWorldDiscCount
            || CancellationRecoveryCallbackCount != 1
            || Golfer->GetPresentationComponent()->GetAnimationPhase()
                != EGolferAnimationPhase::Setup)
        {
            Fail(TEXT("evaluated pre-release cancellation did not restore every authority exactly once"));
            return;
        }

        SetStage(EStage::ObservingCancelledAttempt);
        return;
    }

    if (SecondsInStage() > CancellationPoseTimeoutSeconds)
    {
        Fail(TEXT("cancellation fixture never evaluated into the authored ReachBack phase"));
    }
}

void ADiscGolfSession3SmokeRunner::ObserveCancelledAttempt()
{
    if (ThrowAdapter->IsThrowActive()
        || FrameworkThrowComponent->bThrowActive
        || FrameworkThrowComponent->CurrentPhase != EDGThrowPhase::Idle
        || ThrowAdapter->GetTotalReleaseCommitCount() != BaselineTotalReleaseCount
        || GameMode->GetActiveDisc() != nullptr
        || GameMode->GetStrokes() != BaselineStrokes
        || CountWorldDiscs() != BaselineWorldDiscCount
        || CancellationRecoveryCallbackCount != 1)
    {
        Fail(TEXT("cancelled montage produced a stale release, callback, or gameplay mutation"));
        return;
    }

    if (SecondsInStage() < CancelledReleaseObservationSeconds)
    {
        return;
    }

    if (!GameMode->CanPlayerThrow())
    {
        Fail(TEXT("evaluated pre-release cancellation did not restore the legal gameplay state"));
        return;
    }
    if (!BeginTimingCapture())
    {
        Fail(TEXT("throw controller could not begin the live command after cancellation"));
        return;
    }

    SetStage(EStage::BuildingLiveCommand);
}

void ADiscGolfSession3SmokeRunner::BeginLiveThrow()
{
    if (!FinishTimingCapture(LiveCommand))
    {
        Fail(TEXT("existing throw controller did not produce the live RHBH command"));
        return;
    }
    if (LiveCommand.ThrowStyle != EThrowStyle::Backhand
        || LiveCommand.ShotContext != EDiscShotContext::Drive
        || LiveCommand.Direction.IsNearlyZero())
    {
        Fail(TEXT("live command was not a valid RHBH drive"));
        return;
    }
    if (!Golfer->TryStartAnimatedRHBHThrow(LiveCommand))
    {
        Fail(TEXT("character adapter rejected the live RHBH command"));
        return;
    }

    if (bRequireV006ProductionProfileProof)
    {
        ProductionProfileProofMontagePath = Golfer->GetActiveRHBHThrowMontagePath();
        if (ProductionProfileProofMontagePath
            != FString(DiscGolfProductionMotion::DriveMontage))
        {
            Fail(FString::Printf(
                TEXT("v006 production profile selected the wrong Drive montage (expected=%s actual=%s)"),
                DiscGolfProductionMotion::DriveMontage,
                ProductionProfileProofMontagePath.IsEmpty()
                    ? TEXT("<none>") : *ProductionProfileProofMontagePath));
            return;
        }
    }

    LiveAttemptSerial = ThrowAdapter->GetAttemptSerial();
    LiveRecoveryCallbackCount = 0;
    FollowThroughPhaseEventCount = 0;
    RecoveryPhaseEventCount = 0;
    bSawAnimationActive = ThrowAdapter->IsThrowActive();
    if (!bSawAnimationActive
        || !CommandsMatchExactly(ThrowAdapter->GetLastAuthoritativeCommand(), LiveCommand))
    {
        Fail(TEXT("animation did not activate or the authoritative command changed while being cached"));
        return;
    }
    SetStage(EStage::AwaitingRelease);
}

void ADiscGolfSession3SmokeRunner::ObserveRelease()
{
    if (ThrowAdapter->GetTotalReleaseCommitCount() > BaselineTotalReleaseCount + 1
        || ThrowAdapter->GetReleaseCommitCountForAttempt() > 1)
    {
        Fail(TEXT("release notify committed more than once"));
        return;
    }

    ADiscActor* ActiveDisc = GameMode->GetActiveDisc();
    if (ActiveDisc)
    {
        LaunchedDisc = ActiveDisc;
        bSawRelease = ThrowAdapter->HasCommittedRelease()
            && ThrowAdapter->GetReleaseCommitCountForAttempt() == 1
            && ThrowAdapter->GetTotalReleaseCommitCount() == BaselineTotalReleaseCount + 1
            && ThrowAdapter->WasLastAuthoritativeLaunchAccepted();
        UDiscFlightComponent* Flight = ActiveDisc->GetFlightComponent();
        bSawNonZeroMotion = Flight && Flight->GetVelocityMps().SizeSquared() > FMath::Square(0.01f);
        const FThrowRelease GameplayRelease = GameMode->GetLastRelease();
        const FDiscFlightTelemetry FlightTelemetry = Flight
            ? Flight->GetTelemetry() : FDiscFlightTelemetry();
        bSawAuthoritativeFlight = GameMode->HasLastRelease()
            && GameplayRelease.ReleaseSpeedMps > 0.0f
            && GameplayRelease.SpinRpm > 0.0f
            && FMath::IsNearlyEqual(
                FlightTelemetry.Release.ReleaseSpeedMps, GameplayRelease.ReleaseSpeedMps)
            && FMath::IsNearlyEqual(FlightTelemetry.Release.SpinRpm, GameplayRelease.SpinRpm)
            && FlightTelemetry.Release.ThrowStyle == GameplayRelease.ThrowStyle
            && FlightTelemetry.Release.ShotContext == GameplayRelease.ShotContext
            && GameMode->GetStrokes() == BaselineStrokes + 1;
        bSawPostReleaseAnimationActive = ThrowAdapter->IsThrowActive();
        if (!bSawRelease || !bSawNonZeroMotion || !bSawAuthoritativeFlight
            || !bSawPostReleaseAnimationActive)
        {
            Fail(TEXT("release did not preserve one solver-authored disc and active follow-through"));
            return;
        }
        if (CountWorldDiscs() != BaselineWorldDiscCount + 1)
        {
            Fail(TEXT("world disc count was not exactly one above baseline at release"));
            return;
        }

        SetStage(EStage::AwaitingAnimationRecovery);
        return;
    }

    if (SecondsInStage() > ReleaseTimeoutSeconds)
    {
        Fail(TEXT("DG Release Disc did not reach the gameplay launch authority before timeout"));
    }
}

void ADiscGolfSession3SmokeRunner::ObserveAnimationRecovery()
{
    if (ThrowAdapter->GetTotalReleaseCommitCount() != BaselineTotalReleaseCount + 1
        || ThrowAdapter->GetReleaseCommitCountForAttempt() != 1
        || GameMode->GetStrokes() != BaselineStrokes + 1)
    {
        Fail(TEXT("release count changed during follow-through"));
        return;
    }
    if (GameMode->GetActiveDisc() && GameMode->GetActiveDisc() != LaunchedDisc)
    {
        Fail(TEXT("authoritative gameplay disc was replaced during follow-through"));
        return;
    }
    if (LiveRecoveryCallbackCount > 1)
    {
        Fail(TEXT("live throw recovery event fired more than once"));
        return;
    }

    if (!ThrowAdapter->IsThrowActive())
    {
        if (ThrowAdapter->GetRecoveryReason()
                != EDiscGolfRHBHThrowRecoveryReason::ThrowFinished
            || LiveRecoveryCallbackCount != 1
            || FollowThroughPhaseEventCount != 1
            || RecoveryPhaseEventCount != 1)
        {
            Fail(TEXT("normal recovery did not traverse one FollowThrough and one Recovery phase before DG Throw Finished"));
            return;
        }
        bSawAnimationRecovery = true;
        SetStage(EStage::AwaitingGameplayRecovery);
        return;
    }
    if (SecondsInStage() > AnimationRecoveryTimeoutSeconds)
    {
        Fail(TEXT("DG Throw Finished did not restore the animation/input state before timeout"));
    }
}

void ADiscGolfSession3SmokeRunner::ObserveGameplayRecovery()
{
    if (ThrowAdapter->GetTotalReleaseCommitCount() != BaselineTotalReleaseCount + 1)
    {
        Fail(TEXT("a replacement release was committed after follow-through"));
        return;
    }
    if (LiveRecoveryCallbackCount != 1
        || ThrowAdapter->GetRecoveryReason()
            != EDiscGolfRHBHThrowRecoveryReason::ThrowFinished)
    {
        Fail(TEXT("normal recovery reason or callback cardinality changed after follow-through"));
        return;
    }

    if (GameMode->CanPlayerThrow())
    {
        APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
        const bool bRecovered = !ThrowAdapter->IsThrowActive()
            && GameMode->GetActiveDisc() == nullptr
            && !GameMode->IsBroadcastCameraActive()
            && CountWorldGolfers() == 1
            && PlayerController
            && PlayerController->GetPawn() == Golfer
            && PlayerController->GetViewTarget() == Golfer;
        if (!bRecovered)
        {
            if (SecondsInStage() > GameplayRecoveryTimeoutSeconds)
            {
                Fail(TEXT("camera/player/disc/animation authority did not return to the original pawn"));
            }
            return;
        }

        bSawCameraReturn = true;
        // Use the course-authoritative post-lie context here; this proves the
        // actual next legal action without forcing another Session 3 drive.
        if (!BeginTimingCapture(false) || !Golfer->GetThrowController()->IsTimingActive())
        {
            Fail(TEXT("the recovered player could not begin the next legal throw action"));
            return;
        }
        bSawNextActionBegin = true;
        Golfer->CancelThrowPresentation();
        if (Golfer->GetThrowController()->IsTimingActive()
            || Golfer->GetPresentationComponent()->GetAnimationPhase()
                != EGolferAnimationPhase::Setup
            || !GameMode->CanPlayerThrow())
        {
            Fail(TEXT("the next timing/presentation action did not cancel back to playable control"));
            return;
        }
        Pass();
        return;
    }
    if (SecondsInStage() > GameplayRecoveryTimeoutSeconds)
    {
        Fail(TEXT("flight/lie/camera path did not return the next legal action before timeout"));
    }
}

void ADiscGolfSession3SmokeRunner::HandleThrowRecovered(
    int64 AttemptSerial,
    bool bDiscWasReleased)
{
    (void)bDiscWasReleased;

    if (AttemptSerial == CancellationAttemptSerial)
    {
        ++CancellationRecoveryCallbackCount;
    }
    else if (AttemptSerial == LiveAttemptSerial)
    {
        ++LiveRecoveryCallbackCount;
    }
}

void ADiscGolfSession3SmokeRunner::HandleThrowPhaseChanged(EDGThrowPhase NewPhase)
{
    if (!ThrowAdapter || LiveAttemptSerial <= 0
        || ThrowAdapter->GetAttemptSerial() != LiveAttemptSerial)
    {
        return;
    }

    if (NewPhase == EDGThrowPhase::FollowThrough)
    {
        ++FollowThroughPhaseEventCount;
    }
    else if (NewPhase == EDGThrowPhase::Recovery)
    {
        ++RecoveryPhaseEventCount;
    }
}

int32 ADiscGolfSession3SmokeRunner::CountWorldDiscs() const
{
    int32 Count = 0;
    if (!GetWorld())
    {
        return Count;
    }
    for (TActorIterator<ADiscActor> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && !It->IsActorBeingDestroyed())
        {
            ++Count;
        }
    }
    return Count;
}

int32 ADiscGolfSession3SmokeRunner::CountWorldGolfers() const
{
    int32 Count = 0;
    if (!GetWorld())
    {
        return Count;
    }
    for (TActorIterator<ADiscGolferPawn> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && !It->IsActorBeingDestroyed())
        {
            ++Count;
        }
    }
    return Count;
}

void ADiscGolfSession3SmokeRunner::SetStage(EStage NewStage)
{
    Stage = NewStage;
    StageStartWorldSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

double ADiscGolfSession3SmokeRunner::SecondsInStage() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() - StageStartWorldSeconds : 0.0;
}

void ADiscGolfSession3SmokeRunner::Fail(const FString& Reason)
{
    if (bFinished)
    {
        return;
    }
    bFinished = true;
    Stage = EStage::Finished;
    GetWorldTimerManager().ClearTimer(PollTimer);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("SESSION 3 ONE-THROW SMOKE FAIL: %s | aim=%d animation=%d release=%d motion=%d authoritative_flight=%d post_release_active=%d follow_through=%d recovery_phase=%d recovery=%d camera=%d next_action=%d total_releases=%d"),
        *Reason, bSawAimAvailable ? 1 : 0, bSawAnimationActive ? 1 : 0,
        bSawRelease ? 1 : 0, bSawNonZeroMotion ? 1 : 0,
        bSawAuthoritativeFlight ? 1 : 0, bSawPostReleaseAnimationActive ? 1 : 0,
        FollowThroughPhaseEventCount, RecoveryPhaseEventCount,
        bSawAnimationRecovery ? 1 : 0, bSawCameraReturn ? 1 : 0,
        bSawNextActionBegin ? 1 : 0,
        ThrowAdapter ? ThrowAdapter->GetTotalReleaseCommitCount() - BaselineTotalReleaseCount : -1);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession3SmokeRunner::Pass()
{
    if (bFinished)
    {
        return;
    }
    if (bRequireV006ProductionProfileProof
        && (ProductionProfileProofName.IsEmpty()
            || ProductionProfileProofMontagePath
                != FString(DiscGolfProductionMotion::DriveMontage)))
    {
        Fail(TEXT("v006 production profile proof identity was not retained through recovery"));
        return;
    }
    bFinished = true;
    Stage = EStage::Finished;
    GetWorldTimerManager().ClearTimer(PollTimer);
    if (bRequireV006ProductionProfileProof)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("SESSION 19 V006 PRODUCTION PROFILE PASS: profile=%s recipe=%s revision=%s montage=%s"),
            *ProductionProfileProofName,
            DiscGolfProductionMotion::ActiveVersion,
            DiscGolfProductionMotion::ActiveAssetRevision,
            *ProductionProfileProofMontagePath);
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 3 ONE-THROW SMOKE PASS: aim=1 -> animation=1 -> release=1 -> authoritative_disc=1 -> existing_flight_solver=1 -> nonzero_motion=1 -> FollowThrough_phase=1 -> Recovery_phase=1 -> DG_Throw_Finished=1 -> recovery_callback=1 -> original_player=1 -> camera_return=1 -> next_action_begin_and_cancel=1; evaluated ReachBack cancellation recovered once and launched 0 discs through the former release time."));
    FPlatformMisc::RequestExitWithStatus(false, 0);
}
