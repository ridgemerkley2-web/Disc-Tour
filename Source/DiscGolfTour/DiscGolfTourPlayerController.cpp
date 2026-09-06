#include "DiscGolfTourPlayerController.h"

#include "DiscGolfCharacterCreatorWidget.h"
#include "DiscGolfAvatarBackendProfile.h"
#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolferPawn.h"
#include "DiscGolfInputConfig.h"
#include "DiscGolfPlayerExperience.h"
#include "DiscGolfPlayabilityMonitorComponent.h"
#include "DiscGolfRoundFlowWidget.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTour.h"
#include "ThrowControllerComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "GameplayTagContainer.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
    constexpr int32 PlayerSettingRowCount = 32;

    int32 WrapIndex(int32 Value, int32 Count)
    {
        return Count > 0 ? (Value % Count + Count) % Count : 0;
    }

    bool IsControlsToggleKey(const FKey& Key)
    {
        return Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Left;
    }

    bool IsAnyOf(const FKey& Key, std::initializer_list<FKey> Choices)
    {
        for (const FKey& Choice : Choices)
        {
            if (Key == Choice) return true;
        }
        return false;
    }

    bool RoundFlowSnapshotsEqual(
        const FDGRoundFlowSnapshot& Left,
        const FDGRoundFlowSnapshot& Right)
    {
        return Left.Screen == Right.Screen
            && Left.CourseName == Right.CourseName
            && Left.CurrentHoleIndex == Right.CurrentHoleIndex
            && Left.Rows == Right.Rows
            && Left.CompletedHoleCount == Right.CompletedHoleCount
            && Left.CompletedPar == Right.CompletedPar
            && Left.TotalStrokes == Right.TotalStrokes
            && Left.TotalPenaltyStrokes == Right.TotalPenaltyStrokes
            && Left.ScoreToPar == Right.ScoreToPar
            && Left.AllowedActions == Right.AllowedActions
            && Left.InitialFocusAction == Right.InitialFocusAction;
    }

    void SanitizeCreatorProfile(
        FDGBodyProfile& Body,
        FDGThrowStyle& ThrowStyle,
        EDGHandedness& Handedness)
    {
        FDiscGolfCharacterProfileSaveData Safe =
            FDiscGolfCharacterProfileSaveData::FromFramework(Body, ThrowStyle, Handedness);
        Safe.Sanitize();
        Body = Safe.ToBodyProfile();
        ThrowStyle = Safe.ToThrowStyle();
        Handedness = Safe.GetHandedness();
    }
}

ADiscGolfTourPlayerController::ADiscGolfTourPlayerController() = default;

void ADiscGolfTourPlayerController::BeginPlay()
{
    Super::BeginPlay();
    FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(
        this, &ADiscGolfTourPlayerController::HandleApplicationWillDeactivate);
    FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(
        this, &ADiscGolfTourPlayerController::HandleApplicationHasReactivated);
    EnsureGameplayInputReady();
    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    ApplyMainMenuInputMode(GameMode && GameMode->IsMainMenuVisible());
    RefreshRoundFlowPresentation();
}

void ADiscGolfTourPlayerController::ApplyMainMenuInputMode(bool bMenuVisible)
{
    if (!IsLocalController())
    {
        return;
    }

    EnsureGameplayInputReady();
    SetGameplayContextEnabled(!bMenuVisible);
    bShowMouseCursor = bMenuVisible;
    if (bMenuVisible)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }
    else
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
    }
    FlushPressedKeys();
}

bool ADiscGolfTourPlayerController::BuildRoundFlowState(
    FDGRoundFlowState& OutState,
    FString& OutError) const
{
    OutState = FDGRoundFlowState();
    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    if (!GameMode)
    {
        OutError = TEXT("round-flow presentation requires the authoritative GameMode");
        return false;
    }

    OutState.bMainMenuVisible = GameMode->IsMainMenuVisible();
    OutState.bScorecardVisible = GameMode->IsScorecardVisible();
    OutState.bHoleComplete = GameMode->IsHoleComplete();
    OutState.bRoundComplete = GameMode->IsRoundComplete();

    const FDiscGolfRoundState Round = GameMode->GetRoundState();
    if (!Round.HoleScores.IsEmpty())
    {
        OutState.CourseName = Round.CourseName.ToString();
        OutState.CurrentHoleIndex = Round.CurrentHoleIndex;
        OutState.Rows.Reserve(Round.HoleScores.Num());
        for (const FDiscGolfRoundHoleScore& Score : Round.HoleScores)
        {
            FDGRoundFlowRow& Row = OutState.Rows.AddDefaulted_GetRef();
            Row.HoleNumber = Score.HoleNumber;
            Row.HoleName = Score.HoleName.ToString();
            Row.Par = Score.Par;
            Row.Strokes = Score.Strokes;
            Row.PenaltyStrokes = Score.PenaltyStrokes;
            Row.bCompleted = Score.bCompleted;
            Row.ScoreToPar = Score.ScoreToPar();
        }
        OutState.CompletedHoleCount = DiscGolfRound::CompletedHoleCount(Round);
        OutState.CompletedPar = DiscGolfRound::CompletedPar(Round);
        OutState.TotalStrokes = DiscGolfRound::TotalStrokes(Round);
        OutState.TotalPenaltyStrokes = DiscGolfRound::TotalPenaltyStrokes(Round);
        OutState.ScoreToPar = DiscGolfRound::ScoreToPar(Round);
    }

    OutError.Reset();
    return true;
}

void ADiscGolfTourPlayerController::RemoveRoundFlowWidget()
{
    if (RoundFlowWidget)
    {
        // Round-flow buttons receive Space/South in Slate before the raw game
        // route. Clear their focus synchronously so a detached button cannot
        // retain the key transaction after control returns to gameplay.
        if (FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
        }
        RoundFlowWidget->RemoveFromParent();
        RoundFlowWidget = nullptr;
    }
}

bool ADiscGolfTourPlayerController::HasInteractiveRoundFlowWidget() const
{
    return RoundFlowWidget
        && RoundFlowWidget->IsInViewport()
        && RoundFlowWidget->HasFocusTarget();
}

void ADiscGolfTourPlayerController::ApplyRoundFlowInputMode()
{
    if (!IsLocalController())
    {
        return;
    }

    EnsureGameplayInputReady();
    SetGameplayContextEnabled(false);
    bShowMouseCursor = true;

    FInputModeGameAndUI InputMode;
    const TSharedPtr<SWidget> FocusWidget = HasInteractiveRoundFlowWidget()
        ? RoundFlowWidget->GetInitialFocusWidget() : nullptr;
    if (FocusWidget.IsValid())
    {
        InputMode.SetWidgetToFocus(FocusWidget.ToSharedRef());
    }
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    if (FocusWidget.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetKeyboardFocus(
            FocusWidget, EFocusCause::SetDirectly);
    }
    FlushPressedKeys();
}

void ADiscGolfTourPlayerController::RefreshRoundFlowPresentation()
{
    if (!IsLocalController() || bRefreshingRoundFlowPresentation)
    {
        return;
    }
    TGuardValue<bool> RefreshGuard(bRefreshingRoundFlowPresentation, true);

    FDGRoundFlowState State;
    FDGRoundFlowSnapshot Candidate;
    FString Error;
    if (!BuildRoundFlowState(State, Error)
        || !DiscGolfRoundFlow::Resolve(State, Candidate, Error))
    {
        const bool bHadRoundFlowState = bHasRoundFlowSnapshot || RoundFlowWidget != nullptr;
        RemoveRoundFlowWidget();
        bHasRoundFlowSnapshot = false;
        const bool bErrorChanged = RoundFlowLastError != Error;
        if (bErrorChanged)
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("Round-flow presentation rejected authoritative state: %s"),
                *Error);
            RoundFlowLastError = Error;
        }
        if (State.bMainMenuVisible || State.bScorecardVisible)
        {
            EnsureGameplayInputReady();
            SetGameplayContextEnabled(false);
            bShowMouseCursor = true;
            if (bHadRoundFlowState || bErrorChanged)
            {
                ApplyRoundFlowInputMode();
            }
        }
        return;
    }
    RoundFlowLastError.Reset();

    const bool bSnapshotChanged = !bHasRoundFlowSnapshot
        || !RoundFlowSnapshotsEqual(ActiveRoundFlowSnapshot, Candidate);
    ActiveRoundFlowSnapshot = Candidate;
    bHasRoundFlowSnapshot = true;

    const bool bRoundFlowVisible = Candidate.Screen != EDGRoundFlowScreen::Hidden;
    if (bRoundFlowVisible && !bControlsMenuOpen && !bCharacterCreatorOpen)
    {
        EnsureGameplayInputReady();
        SetGameplayContextEnabled(false);
        bShowMouseCursor = true;
    }
    if (!bRoundFlowVisible || bControlsMenuOpen || bCharacterCreatorOpen)
    {
        const bool bRemovedWidget = RoundFlowWidget != nullptr;
        RemoveRoundFlowWidget();
        if (!bRoundFlowVisible)
        {
            bRoundFlowWidgetCreationAttempted = false;
            RoundFlowWidgetAttemptedScreen = EDGRoundFlowScreen::Hidden;
            if (!bControlsMenuOpen && !bCharacterCreatorOpen
                && (bSnapshotChanged || bRemovedWidget))
            {
                SetGameplayContextEnabled(true);
                bShowMouseCursor = false;
                FInputModeGameOnly InputMode;
                SetInputMode(InputMode);
                if (bSnapshotChanged)
                {
                    FlushPressedKeys();
                }
            }
        }
        return;
    }

    bool bWidgetCreated = false;
    if (!RoundFlowWidget
        && (!bRoundFlowWidgetCreationAttempted
            || RoundFlowWidgetAttemptedScreen != Candidate.Screen))
    {
        bRoundFlowWidgetCreationAttempted = true;
        RoundFlowWidgetAttemptedScreen = Candidate.Screen;
        RoundFlowWidget = CreateWidget<UDiscGolfRoundFlowWidget>(
            this, UDiscGolfRoundFlowWidget::StaticClass());
        if (RoundFlowWidget)
        {
            RoundFlowWidget->InitializeForController(this);
            bWidgetCreated = true;
        }
    }

    const bool bNeedsWidgetRefresh = RoundFlowWidget
        && (bWidgetCreated || bSnapshotChanged
            || !RoundFlowWidget->IsInViewport()
            || !RoundFlowWidget->HasFocusTarget());
    const bool bWidgetReady = !bNeedsWidgetRefresh
        || (RoundFlowWidget->ApplySnapshot(Candidate, &Error)
            && (RoundFlowWidget->IsInViewport()
                || RoundFlowWidget->AddToPlayerScreen(900))
            && RoundFlowWidget->HasFocusTarget());
    if (!bWidgetReady)
    {
        if (RoundFlowLastError != Error)
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("Round-flow widget fell back to Canvas: %s"),
                Error.IsEmpty() ? TEXT("widget attach/focus target unavailable") : *Error);
            RoundFlowLastError = Error;
        }
        RemoveRoundFlowWidget();
    }

    if (bWidgetCreated || bSnapshotChanged)
    {
        ApplyRoundFlowInputMode();
    }
}

bool ADiscGolfTourPlayerController::OpenSettingsFromRoundFlow()
{
    RefreshRoundFlowPresentation();
    if (bControlsMenuOpen || bCharacterCreatorOpen || !bHasRoundFlowSnapshot
        || ActiveRoundFlowSnapshot.Screen == EDGRoundFlowScreen::Hidden)
    {
        return false;
    }

    RoundFlowSettingsOrigin = ActiveRoundFlowSnapshot.Screen;
    bRoundFlowSettingsOriginValid = true;
    bSettingsPage = true;
    RemoveRoundFlowWidget();
    bRoundFlowWidgetCreationAttempted = false;
    RoundFlowWidgetAttemptedScreen = EDGRoundFlowScreen::Hidden;
    OpenControlsMenu();
    return bControlsMenuOpen && !bGameplayContextAdded;
}

bool ADiscGolfTourPlayerController::HandleRoundFlowAction(
    EDGRoundFlowAction Action)
{
    if (bHandlingRoundFlowAction)
    {
        return false;
    }
    TGuardValue<bool> ActionGuard(bHandlingRoundFlowAction, true);

    RefreshRoundFlowPresentation();
    if (!bHasRoundFlowSnapshot
        || !DiscGolfRoundFlow::IsActionAllowed(ActiveRoundFlowSnapshot, Action))
    {
        return false;
    }

    ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    if (!GameMode)
    {
        return false;
    }

    TArray<FKey> PressedTransitionKeys;
    const bool bTransitionsToGameplay =
        DiscGolfRoundFlow::TransitionsToGameplay(Action);
    if (bTransitionsToGameplay)
    {
        // Slate may dispatch the focused button while its keyboard/controller
        // confirm is still down. Capture that physical ownership before the
        // synchronous UI-to-gameplay transition re-enables Enhanced Input.
        GetPressedPresentationTransitionKeys(PressedTransitionKeys);
    }

    bool bHandled = false;
    switch (Action)
    {
        case EDGRoundFlowAction::StartOrContinue:
            bHandled = GameMode->StartOrContinueFromMainMenu();
            break;
        case EDGRoundFlowAction::OpenSettings:
            bHandled = OpenSettingsFromRoundFlow();
            break;
        case EDGRoundFlowAction::CloseScorecard:
            GameMode->ToggleScorecard();
            bHandled = !GameMode->IsScorecardVisible();
            break;
        case EDGRoundFlowAction::AdvanceOrRestart:
            if (ActiveRoundFlowSnapshot.Screen == EDGRoundFlowScreen::RoundResults)
            {
                GameMode->RestartRound();
                bHandled = !GameMode->IsRoundComplete();
            }
            else
            {
                const int32 PreviousHoleIndex = GameMode->GetRoundState().CurrentHoleIndex;
                GameMode->AdvanceToNextHole();
                bHandled = GameMode->GetRoundState().CurrentHoleIndex != PreviousHoleIndex;
            }
            break;
        case EDGRoundFlowAction::ReturnToMainMenu:
            bHandled = GameMode->ReturnToMainMenu();
            break;
        case EDGRoundFlowAction::None:
        default:
            return false;
    }

    if (bHandled && bTransitionsToGameplay)
    {
        // Even a non-throw navigation command (for example N) can remove a
        // focused Slate button while a later shared-confirm event is still
        // reconciled. Require a raw, explicit throw down after every UI to
        // gameplay handoff, whether or not a protected key is currently down.
        bFreshThrowDownRequiredAfterPresentation = true;
        for (const FKey& Key : PressedTransitionKeys)
        {
            ArmPresentationTransitionKey(Key);
        }
    }

    RefreshRoundFlowPresentation();
    return bHandled;
}

bool ADiscGolfTourPlayerController::RunRoundFlowSettingsRecoveryProbe(
    FString& OutError)
{
    RefreshRoundFlowPresentation();
    if (!bHasRoundFlowSnapshot
        || ActiveRoundFlowSnapshot.Screen == EDGRoundFlowScreen::Hidden
        || !HasInteractiveRoundFlowWidget())
    {
        OutError = TEXT("round-flow settings probe requires an interactive visible origin");
        return false;
    }

    const EDGRoundFlowScreen ExpectedOrigin = ActiveRoundFlowSnapshot.Screen;
    if (!OpenSettingsFromRoundFlow()
        || !bControlsMenuOpen || RoundFlowWidget || bGameplayContextAdded)
    {
        OutError = TEXT("round-flow settings probe could not enter isolated settings");
        return false;
    }

    CloseControlsMenu();
    const bool bRecovered = !bControlsMenuOpen
        && bHasRoundFlowSnapshot
        && ActiveRoundFlowSnapshot.Screen == ExpectedOrigin
        && HasInteractiveRoundFlowWidget()
        && bShowMouseCursor
        && GetActiveInputRoute() == EDiscGolfInputRoute::UI
        && !bGameplayContextAdded;
    if (!bRecovered)
    {
        OutError = TEXT("settings close did not restore the originating round-flow input/focus state");
        return false;
    }

    OutError.Reset();
    return true;
}

bool ADiscGolfTourPlayerController::RunPlayabilityPauseResumeProbe(FString& OutError)
{
    if (!GetWorld() || !IsLocalController() || bControlsMenuOpen
        || bCharacterCreatorOpen || !bGameplayContextAdded || GetWorld()->IsPaused())
    {
        OutError = TEXT("temporary-state probe requires unpaused gameplay input ownership");
        return false;
    }

    OpenControlsMenu();
    const bool bPausedStateValid = bControlsMenuOpen
        && GetWorld()->IsPaused()
        && !bGameplayContextAdded;
    CloseControlsMenu();
    const bool bRecovered = !bControlsMenuOpen
        && !GetWorld()->IsPaused()
        && bGameplayContextAdded
        && GetActiveInputRoute() == EDiscGolfInputRoute::Gameplay;
    if (!bPausedStateValid || !bRecovered)
    {
        OutError = FString::Printf(
            TEXT("controls pause/input recovery failed (paused=%s recovered=%s)"),
            bPausedStateValid ? TEXT("yes") : TEXT("no"),
            bRecovered ? TEXT("yes") : TEXT("no"));
        return false;
    }

    OutError.Reset();
    return true;
}

void ADiscGolfTourPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
    FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
    PresentationDismissInputBarrier.Reset();
    PendingPresentationDismissReleaseCompletions.Reset();
    bRecoverPresentationDismissAfterReactivation = false;
    bPresentationDismissAwaitingReactivationNeutral = false;
    PresentationDismissReactivationFrame = 0;
    bFreshThrowDownRequiredAfterPresentation = false;
    RemoveRoundFlowWidget();
    bHasRoundFlowSnapshot = false;
    bRoundFlowSettingsOriginValid = false;
    if (bCharacterCreatorOpen)
    {
        CloseCharacterCreator(true);
    }

    if (bGameplayContextAdded && EffectiveInputConfig && EffectiveInputConfig->GameplayMappingContext)
    {
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                Subsystem->RemoveMappingContext(EffectiveInputConfig->GameplayMappingContext);
            }
        }
    }

    bGameplayContextAdded = false;
    Super::EndPlay(EndPlayReason);
}

bool ADiscGolfTourPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    const bool bPressed = Params.Event == IE_Pressed;
    const bool bSharedConfirmKey = IsAnyOf(
        Params.Key, {EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom});
    ADiscGolfTourGameMode* TourGameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    RefreshRoundFlowPresentation();
    const bool bRoundFlowOwnsInput = bHasRoundFlowSnapshot
        && ActiveRoundFlowSnapshot.Screen != EDGRoundFlowScreen::Hidden;
    const bool bTopLevelUiOwnsInput = bCharacterCreatorOpen
        || bControlsMenuOpen
        || bRoundFlowOwnsInput
        || (TourGameMode && (
            TourGameMode->IsMainMenuVisible()
            || TourGameMode->IsScorecardVisible()));
    const bool bBarrierProtectedKey = bSharedConfirmKey
        || IsPresentationTransitionProtectedKey(Params.Key);
    const bool bHoleIntroOwnsSharedConfirm = bSharedConfirmKey
        && TourGameMode
        && TourGameMode->IsHoleIntroVisible()
        && !bCharacterCreatorOpen
        && !bControlsMenuOpen
        && !TourGameMode->IsMainMenuVisible()
        && !TourGameMode->IsScorecardVisible();
    const EDiscGolfPresentationDismissInputDisposition DismissDisposition =
        PresentationDismissInputBarrier.Route(
            Params.Key,
            Params.Event,
            bBarrierProtectedKey,
            bHoleIntroOwnsSharedConfirm);
    const bool bExplicitFreshThrowDown = IsCurrentThrowInputKey(Params.Key)
        && (Params.Event == IE_Pressed || Params.Event == IE_DoubleClick);
    if (bFreshThrowDownRequiredAfterPresentation
        && bExplicitFreshThrowDown
        && DismissDisposition == EDiscGolfPresentationDismissInputDisposition::PassThrough
        && !PresentationDismissInputBarrier.IsPending()
        && !bRecoverPresentationDismissAfterReactivation
        && !bTopLevelUiOwnsInput)
    {
        // Clear the gameplay latch synchronously on the raw, explicit down edge
        // so the corresponding Enhanced Input Started event may throw. Repeat
        // reconciliation and delayed callbacks cannot manufacture fresh intent.
        bFreshThrowDownRequiredAfterPresentation = false;
    }
    if (DismissDisposition == EDiscGolfPresentationDismissInputDisposition::DismissAndConsume)
    {
        // Disable the action mapping before the presentation gate opens. The
        // matching release is still received by this raw controller override.
        // Join every throw binding that was already physically down so that
        // Space cannot reopen gameplay over a held controller/remapped key.
        TArray<FKey> AlreadyPressedTransitionKeys;
        GetPressedPresentationTransitionKeys(AlreadyPressedTransitionKeys);
        for (const FKey& Key : AlreadyPressedTransitionKeys)
        {
            PresentationDismissInputBarrier.Arm(Key);
        }
        bFreshThrowDownRequiredAfterPresentation = true;
        SetGameplayContextEnabled(false);
        FlushPressedKeys();
        TourGameMode->SkipCurrentPresentation();
        return true;
    }
    if (DismissDisposition == EDiscGolfPresentationDismissInputDisposition::ConsumeHeldEdge)
    {
        return true;
    }
    if (DismissDisposition == EDiscGolfPresentationDismissInputDisposition::ForwardReleaseAndConsume)
    {
        // Record the up edge while both logical and Enhanced Input gameplay
        // routes remain closed. TimerManager runs before Enhanced Input's
        // deferred mapping rebuild, so a next-tick timer can coalesce the
        // remove and re-add into one still-active context. PostProcessInput
        // owns completion after a full later neutral input frame instead.
        Super::InputKey(Params);
        FlushPressedKeys();
        const uint32 ReleaseGeneration =
            PresentationDismissInputBarrier.GetReleaseGeneration(Params.Key);
        if (ReleaseGeneration != 0)
        {
            FPendingPresentationDismissReleaseCompletion& Completion =
                PendingPresentationDismissReleaseCompletions.FindOrAdd(Params.Key);
            Completion.Generation = ReleaseGeneration;
            Completion.ReleaseFrame = GFrameCounter;
        }
        return true;
    }

    if (bCharacterCreatorOpen)
    {
        if (bPressed && IsControlsToggleKey(Params.Key))
        {
            CancelCharacterCreator();
            return true;
        }

        // Gameplay mapping is disabled while the creator is open. Let Slate
        // receive navigation, slider, button and controller-focus input.
        return Super::InputKey(Params);
    }

    if (bControlsMenuOpen)
    {
        if (bWaitingForControlBinding)
        {
            if (bPressed && IsControlsToggleKey(Params.Key))
            {
                CancelControlBindingCapture();
            }
            else if (bPressed || (Params.Event == IE_Axis && Params.Key.IsGamepadKey() && FMath::Abs(Params.AmountDepressed) >= 0.55f))
            {
                TryApplyControlBinding(Params.Key);
            }
            return true;
        }

        if (bPressed)
        {
            if (IsControlsToggleKey(Params.Key))
            {
                CloseControlsMenu();
            }
            else if (IsAnyOf(Params.Key, {EKeys::Tab, EKeys::Gamepad_RightShoulder}))
            {
                ToggleMenuPage();
            }
            else if (IsAnyOf(Params.Key, {EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up}))
            {
                bSettingsPage ? MoveSettingsSelection(-1) : MoveControlSelection(-1);
            }
            else if (IsAnyOf(Params.Key, {EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down}))
            {
                bSettingsPage ? MoveSettingsSelection(1) : MoveControlSelection(1);
            }
            else if (IsAnyOf(Params.Key, {EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left}))
            {
                bSettingsPage ? AdjustSelectedSetting(-1) : MoveBindingSelection(-1);
            }
            else if (IsAnyOf(Params.Key, {EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right}))
            {
                bSettingsPage ? AdjustSelectedSetting(1) : MoveBindingSelection(1);
            }
            else if (IsAnyOf(Params.Key, {EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom}))
            {
                if (bSettingsPage) OpenCharacterCreator();
                else BeginControlBindingCapture();
            }
            else if (IsAnyOf(Params.Key, {EKeys::R, EKeys::Gamepad_FaceButton_Top}))
            {
                ResetSelectedControl();
            }
            else if (IsAnyOf(Params.Key, {EKeys::BackSpace, EKeys::Gamepad_LeftShoulder}))
            {
                ResetAllControls();
            }
        }
        return true;
    }

    if (bHasRoundFlowSnapshot
        && ActiveRoundFlowSnapshot.Screen != EDGRoundFlowScreen::Hidden)
    {
        if (bPressed && IsControlsToggleKey(Params.Key))
        {
            OpenSettingsFromRoundFlow();
            return true;
        }
        if (bPressed
            && IsAnyOf(Params.Key, {EKeys::Tab, EKeys::Gamepad_LeftTrigger})
            && DiscGolfRoundFlow::IsActionAllowed(
                ActiveRoundFlowSnapshot,
                EDGRoundFlowAction::CloseScorecard))
        {
            HandleRoundFlowAction(EDGRoundFlowAction::CloseScorecard);
            return true;
        }
        if (bPressed
            && IsAnyOf(Params.Key, {EKeys::N, EKeys::Gamepad_RightTrigger})
            && DiscGolfRoundFlow::IsActionAllowed(
                ActiveRoundFlowSnapshot,
                EDGRoundFlowAction::AdvanceOrRestart))
        {
            HandleRoundFlowAction(EDGRoundFlowAction::AdvanceOrRestart);
            return true;
        }
        if (bPressed
            && IsAnyOf(Params.Key, {EKeys::M, EKeys::Gamepad_FaceButton_Right})
            && DiscGolfRoundFlow::IsActionAllowed(
                ActiveRoundFlowSnapshot,
                EDGRoundFlowAction::ReturnToMainMenu))
        {
            HandleRoundFlowAction(EDGRoundFlowAction::ReturnToMainMenu);
            return true;
        }

        // An attached native widget owns ordinary confirm/navigation. If it
        // could not be created, the same default action remains available to
        // the Canvas fallback through raw keyboard/controller input.
        if (HasInteractiveRoundFlowWidget())
        {
            return Super::InputKey(Params);
        }
        if (bPressed && IsAnyOf(Params.Key,
            {EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom}))
        {
            const EDGRoundFlowAction Action = ActiveRoundFlowSnapshot.InitialFocusAction;
            if (HandleRoundFlowAction(Action)
                && Action == EDGRoundFlowAction::StartOrContinue)
            {
                // The Canvas fallback handles confirm before Super::InputKey,
                // so explicitly quarantine its physical key until release.
                ArmPresentationTransitionKey(Params.Key);
            }
        }
        return true;
    }

    if (bPressed && Params.Key == EKeys::F9)
    {
        if (ADiscGolfTourGameMode* GameMode = GetWorld()
            ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr)
        {
            GameMode->ToggleDeveloperHud();
        }
        return true;
    }

    if (bPressed && GetActiveInputRoute() == EDiscGolfInputRoute::Replay)
    {
        ADiscGolfTourGameMode* GameMode = GetWorld()
            ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
        if (GameMode && IsAnyOf(Params.Key,
            {EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom}))
        {
            GameMode->ToggleReplayPause();
            return true;
        }
        if (GameMode && IsAnyOf(Params.Key,
            {EKeys::Left, EKeys::Gamepad_DPad_Left}))
        {
            GameMode->SeekReplayRelative(-1.0f);
            return true;
        }
        if (GameMode && IsAnyOf(Params.Key,
            {EKeys::Right, EKeys::Gamepad_DPad_Right}))
        {
            GameMode->SeekReplayRelative(1.0f);
            return true;
        }
        if (GameMode && IsAnyOf(Params.Key,
            {EKeys::Up, EKeys::Gamepad_FaceButton_Top}))
        {
            GameMode->CycleReplayPlaybackRate();
            return true;
        }
    }

    if (bPressed && IsControlsToggleKey(Params.Key))
    {
        OpenControlsMenu();
        return true;
    }

    return Super::InputKey(Params);
}

void ADiscGolfTourPlayerController::PostProcessInput(
    const float DeltaTime,
    const bool bGamePaused)
{
    Super::PostProcessInput(DeltaTime, bGamePaused);

    if (bRecoverPresentationDismissAfterReactivation
        && bPresentationDismissAwaitingReactivationNeutral
        && GFrameCounter > PresentationDismissReactivationFrame)
    {
        TArray<FKey> ProtectedKeys;
        GetPresentationTransitionProtectedKeys(ProtectedKeys);
        bool bAllProtectedKeysNeutral = PlayerInput != nullptr;
        if (PlayerInput)
        {
            for (const FKey& Key : ProtectedKeys)
            {
                if (PlayerInput->IsPressed(Key))
                {
                    bAllProtectedKeysNeutral = false;
                    break;
                }
            }
        }
        if (bAllProtectedKeysNeutral)
        {
            bRecoverPresentationDismissAfterReactivation = false;
            bPresentationDismissAwaitingReactivationNeutral = false;
            PresentationDismissReactivationFrame = 0;
            FlushPressedKeys();
            RestoreGameplayContextAfterPresentationDismiss();
        }
    }

    if (PendingPresentationDismissReleaseCompletions.IsEmpty())
    {
        return;
    }

    TArray<FKey> StaleKeys;
    TArray<FKey> NeutralKeys;
    for (const TPair<FKey, FPendingPresentationDismissReleaseCompletion>& Pair :
        PendingPresentationDismissReleaseCompletions)
    {
        const uint32 CurrentGeneration =
            PresentationDismissInputBarrier.GetReleaseGeneration(Pair.Key);
        if (CurrentGeneration == 0 || CurrentGeneration != Pair.Value.Generation)
        {
            StaleKeys.Add(Pair.Key);
            continue;
        }
        if (GFrameCounter <= Pair.Value.ReleaseFrame
            || (PlayerInput && PlayerInput->IsPressed(Pair.Key)))
        {
            continue;
        }
        NeutralKeys.Add(Pair.Key);
    }

    for (const FKey& Key : StaleKeys)
    {
        PendingPresentationDismissReleaseCompletions.Remove(Key);
    }
    for (const FKey& Key : NeutralKeys)
    {
        const FPendingPresentationDismissReleaseCompletion Completion =
            PendingPresentationDismissReleaseCompletions.FindAndRemoveChecked(Key);
        CompletePresentationDismissRelease(Key, Completion.Generation);
    }
}

void ADiscGolfTourPlayerController::HandleApplicationWillDeactivate()
{
    if (!IsLocalController()
        || (!PresentationDismissInputBarrier.IsPending()
            && !bFreshThrowDownRequiredAfterPresentation))
    {
        return;
    }

    // Platform focus loss flushes PlayerInput without routing a physical
    // IE_Released edge through this controller. Treat deactivation as a
    // trusted neutral boundary, but leave gameplay disabled until the app is
    // active again and every platform key state has been flushed.
    bRecoverPresentationDismissAfterReactivation = true;
    bPresentationDismissAwaitingReactivationNeutral = false;
    PresentationDismissReactivationFrame = 0;
    PendingPresentationDismissReleaseCompletions.Reset();
    PresentationDismissInputBarrier.Reset();
    SetGameplayContextEnabled(false);
    FlushPressedKeys();
}

void ADiscGolfTourPlayerController::HandleApplicationHasReactivated()
{
    if (!bRecoverPresentationDismissAfterReactivation)
    {
        return;
    }

    bPresentationDismissAwaitingReactivationNeutral = true;
    PresentationDismissReactivationFrame = GFrameCounter;
    FlushPressedKeys();
}

const UDiscGolfInputConfig* ADiscGolfTourPlayerController::EnsureGameplayInputReady()
{
    if (!EffectiveInputConfig)
    {
        FString MissingField;
        if (GameplayInputConfig && GameplayInputConfig->IsComplete(MissingField))
        {
            EffectiveInputConfig = GameplayInputConfig;
        }
        else
        {
            const FString Reason = GameplayInputConfig
                ? FString::Printf(TEXT("asset %s is missing %s"), *GameplayInputConfig->GetPathName(), *MissingField)
                : TEXT("no input config asset is assigned");

            const FDiscGolfPlayerSettings Settings = GetPlayerSettings();
            EffectiveInputConfig = UDiscGolfInputConfig::BuildRuntimeFallback(
                this, Settings.bSouthpawController, Settings.ControllerDeadZone);
            bUsingRuntimeInputFallback = true;
            bAppliedSouthpawController = Settings.bSouthpawController;
            AppliedControllerDeadZone = Settings.ControllerDeadZone;
            if (!bReportedInputFallback)
            {
                const FString Message = FString::Printf(
                    TEXT("Enhanced Input fallback active: %s. Keyboard, mouse, and controller defaults were generated safely."),
                    *Reason);
                UE_LOG(LogDiscGolfTour, Warning, TEXT("%s"), *Message);
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(INDEX_NONE, 8.0f, FColor::Yellow, Message);
                }
                bReportedInputFallback = true;
            }
        }
    }

    if (!EffectiveInputConfig)
    {
        UE_LOG(LogDiscGolfTour, Error, TEXT("Enhanced Input could not create a gameplay input config."));
        return nullptr;
    }

    if (!bGameplayContextAdded && IsLocalController()
        && !PresentationDismissInputBarrier.IsPending()
        && !bRecoverPresentationDismissAfterReactivation)
    {
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (UEnhancedInputUserSettings* UserSettings = Subsystem->GetUserSettings())
                {
                    if (!UserSettings->IsMappingContextRegistered(EffectiveInputConfig->GameplayMappingContext))
                    {
                        UserSettings->RegisterInputMappingContext(EffectiveInputConfig->GameplayMappingContext);
                    }
                }
                Subsystem->AddMappingContext(EffectiveInputConfig->GameplayMappingContext, 0);
                bGameplayContextAdded = true;
            }
        }
    }

    return EffectiveInputConfig;
}

UEnhancedInputLocalPlayerSubsystem* ADiscGolfTourPlayerController::GetEnhancedInputSubsystem() const
{
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        return LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    }
    return nullptr;
}

UEnhancedInputUserSettings* ADiscGolfTourPlayerController::GetEnhancedInputUserSettings() const
{
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem())
    {
        return Subsystem->GetUserSettings();
    }
    return nullptr;
}

void ADiscGolfTourPlayerController::GetPresentationTransitionProtectedKeys(
    TArray<FKey>& OutKeys) const
{
    OutKeys.Reset();
    OutKeys.Add(EKeys::SpaceBar);
    OutKeys.Add(EKeys::Gamepad_FaceButton_Bottom);
    if (!EffectiveInputConfig || !EffectiveInputConfig->ThrowAction)
    {
        return;
    }

    const UInputAction* ThrowAction = EffectiveInputConfig->ThrowAction;
    const UPlayerMappableKeySettings* MappingSettings =
        ThrowAction->GetPlayerMappableKeySettings();
    const FName MappingName = MappingSettings
        && !MappingSettings->GetMappingName().IsNone()
        ? MappingSettings->GetMappingName()
        : ThrowAction->GetFName();
    TArray<const FPlayerKeyMapping*> PlayerMappings;
    GetSortedMappings(MappingName, PlayerMappings);
    for (const FPlayerKeyMapping* Mapping : PlayerMappings)
    {
        if (Mapping && Mapping->GetCurrentKey().IsValid())
        {
            OutKeys.AddUnique(Mapping->GetCurrentKey());
        }
    }

    if (EffectiveInputConfig->GameplayMappingContext)
    {
        for (const FEnhancedActionKeyMapping& Mapping :
            EffectiveInputConfig->GameplayMappingContext->GetMappings())
        {
            if (Mapping.Action == ThrowAction && Mapping.Key.IsValid())
            {
                OutKeys.AddUnique(Mapping.Key);
            }
        }
    }
}

bool ADiscGolfTourPlayerController::IsPresentationTransitionProtectedKey(
    const FKey& Key) const
{
    TArray<FKey> ProtectedKeys;
    GetPresentationTransitionProtectedKeys(ProtectedKeys);
    return ProtectedKeys.Contains(Key);
}

bool ADiscGolfTourPlayerController::IsCurrentThrowInputKey(const FKey& Key) const
{
    if (!EffectiveInputConfig || !EffectiveInputConfig->ThrowAction)
    {
        return Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
    }

    const UInputAction* ThrowAction = EffectiveInputConfig->ThrowAction;
    const UPlayerMappableKeySettings* MappingSettings =
        ThrowAction->GetPlayerMappableKeySettings();
    const FName MappingName = MappingSettings
        && !MappingSettings->GetMappingName().IsNone()
        ? MappingSettings->GetMappingName()
        : ThrowAction->GetFName();
    TArray<const FPlayerKeyMapping*> PlayerMappings;
    GetSortedMappings(MappingName, PlayerMappings);
    bool bHasCurrentPlayerMapping = false;
    for (const FPlayerKeyMapping* Mapping : PlayerMappings)
    {
        if (!Mapping || !Mapping->GetCurrentKey().IsValid())
        {
            continue;
        }
        bHasCurrentPlayerMapping = true;
        if (Mapping->GetCurrentKey() == Key)
        {
            return true;
        }
    }
    if (bHasCurrentPlayerMapping)
    {
        return false;
    }

    if (EffectiveInputConfig->GameplayMappingContext)
    {
        for (const FEnhancedActionKeyMapping& Mapping :
            EffectiveInputConfig->GameplayMappingContext->GetMappings())
        {
            if (Mapping.Action == ThrowAction && Mapping.Key == Key)
            {
                return true;
            }
        }
    }
    return false;
}

void ADiscGolfTourPlayerController::GetPressedPresentationTransitionKeys(
    TArray<FKey>& OutKeys) const
{
    TArray<FKey> ProtectedKeys;
    GetPresentationTransitionProtectedKeys(ProtectedKeys);
    OutKeys.Reset();
    for (const FKey& Key : ProtectedKeys)
    {
        if (IsInputKeyDown(Key))
        {
            OutKeys.Add(Key);
        }
    }
}

void ADiscGolfTourPlayerController::ArmPresentationTransitionKey(const FKey& Key)
{
    if (!IsPresentationTransitionProtectedKey(Key))
    {
        return;
    }
    PresentationDismissInputBarrier.Arm(Key);
    bFreshThrowDownRequiredAfterPresentation = true;
    SetGameplayContextEnabled(false);
    FlushPressedKeys();
}

void ADiscGolfTourPlayerController::SetGameplayContextEnabled(bool bEnabled)
{
    if (!EffectiveInputConfig || !EffectiveInputConfig->GameplayMappingContext) return;
    if (bEnabled && (PresentationDismissInputBarrier.IsPending()
        || bRecoverPresentationDismissAfterReactivation)) return;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem())
    {
        if (bEnabled && !bGameplayContextAdded)
        {
            if (UEnhancedInputUserSettings* UserSettings = Subsystem->GetUserSettings();
                UserSettings && !UserSettings->IsMappingContextRegistered(EffectiveInputConfig->GameplayMappingContext))
            {
                UserSettings->RegisterInputMappingContext(EffectiveInputConfig->GameplayMappingContext);
            }
            Subsystem->AddMappingContext(EffectiveInputConfig->GameplayMappingContext, 0);
            bGameplayContextAdded = true;
        }
        else if (!bEnabled && bGameplayContextAdded)
        {
            Subsystem->RemoveMappingContext(EffectiveInputConfig->GameplayMappingContext);
            bGameplayContextAdded = false;
        }
    }
}

void ADiscGolfTourPlayerController::CompletePresentationDismissRelease(
    FKey ReleasedKey,
    uint32 ReleaseGeneration)
{
    if (!PresentationDismissInputBarrier.CompleteRelease(
            ReleasedKey, ReleaseGeneration))
    {
        return;
    }
    RestoreGameplayContextAfterPresentationDismiss();
}

void ADiscGolfTourPlayerController::RestoreGameplayContextAfterPresentationDismiss()
{
    if (PresentationDismissInputBarrier.IsPending()) return;

    RefreshRoundFlowPresentation();
    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    const bool bRoundFlowVisible = bHasRoundFlowSnapshot
        && ActiveRoundFlowSnapshot.Screen != EDGRoundFlowScreen::Hidden;
    const bool bUiOwnsInput = bControlsMenuOpen
        || bCharacterCreatorOpen
        || bRoundFlowVisible
        || (GameMode && (GameMode->IsMainMenuVisible() || GameMode->IsScorecardVisible()));
    if (!bUiOwnsInput)
    {
        FlushPressedKeys();
        SetGameplayContextEnabled(true);
    }
}

EDiscGolfInputRoute ADiscGolfTourPlayerController::GetActiveInputRoute() const
{
    FDiscGolfInputRouteContext Context;
    Context.bCharacterCreatorOpen = bCharacterCreatorOpen;

    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    Context.bReplayActive = GameMode && GameMode->IsInstantReplayActive();
    Context.bUiOpen = PresentationDismissInputBarrier.IsPending()
        || bControlsMenuOpen || (GameMode && (
        GameMode->IsMainMenuVisible()
        || GameMode->IsScorecardVisible()
        || GameMode->IsHoleIntroVisible()
        || GameMode->IsCourseFlyoverActive()));

    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    Context.bAimThrowActive = Golfer && Golfer->GetThrowController()
        && Golfer->GetThrowController()->IsTimingActive();
    return DiscGolfInputRoutePolicy::Resolve(Context);
}

bool ADiscGolfTourPlayerController::IsInputRouteAllowed(EDiscGolfInputRoute ActionRoute) const
{
    return DiscGolfInputRoutePolicy::AllowsAction(GetActiveInputRoute(), ActionRoute);
}

void ADiscGolfTourPlayerController::RefreshRuntimeInputFromPlayerSettings()
{
    if (!bUsingRuntimeInputFallback || !EffectiveInputConfig)
    {
        return;
    }

    const FDiscGolfPlayerSettings Settings = GetPlayerSettings();
    if (Settings.bSouthpawController == bAppliedSouthpawController
        && FMath::IsNearlyEqual(Settings.ControllerDeadZone, AppliedControllerDeadZone))
    {
        return;
    }

    const bool bRestoreGameplayContext = bGameplayContextAdded;
    SetGameplayContextEnabled(false);
    EffectiveInputConfig = UDiscGolfInputConfig::BuildRuntimeFallback(
        this, Settings.bSouthpawController, Settings.ControllerDeadZone);
    bAppliedSouthpawController = Settings.bSouthpawController;
    AppliedControllerDeadZone = Settings.ControllerDeadZone;

    if (bRestoreGameplayContext && !bControlsMenuOpen && !bCharacterCreatorOpen)
    {
        SetGameplayContextEnabled(true);
    }
}

bool ADiscGolfTourPlayerController::OpenCharacterCreator()
{
    if (bCharacterCreatorOpen)
    {
        if (CharacterCreatorWidget
            && CharacterCreatorWidget->IsInViewport())
        {
            return true;
        }
        if (!CharacterCreatorWidget
            || !CharacterCreatorWidget->AddToPlayerScreen(1000))
        {
            ControlsStatusText = TEXT(
                "Character creator remains safely active, but its screen attach still failed; retry Open or use Cancel to exit.");
            CharacterCreatorStatusText = ControlsStatusText;
            return false;
        }
        CharacterCreatorStatusText = TEXT(
            "Character creator screen attach recovered; the existing safe preview session remains active.");
        ControlsStatusText = CharacterCreatorStatusText;
        CommitCharacterCreatorInputMode();
        return true;
    }

    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    if (!Golfer || !Golfer->IsCharacterProfileChangeSafe())
    {
        ControlsStatusText = TEXT("Character creator unavailable while a throw, flight, replay or transition is active.");
        CharacterCreatorStatusText = ControlsStatusText;
        return false;
    }

    if (!Golfer->GetCharacterCreatorProfile(
            CharacterCreatorOpeningBody,
            CharacterCreatorOpeningThrowStyle,
            CharacterCreatorOpeningHandedness))
    {
        ControlsStatusText = TEXT("Character creator could not read the active profile; no values were changed.");
        CharacterCreatorStatusText = ControlsStatusText;
        return false;
    }
    SanitizeCreatorProfile(
        CharacterCreatorOpeningBody,
        CharacterCreatorOpeningThrowStyle,
        CharacterCreatorOpeningHandedness);
    CharacterCreatorOpeningOutfit = DiscGolfOutfitRuntime::NormalizeForPersistence(
        Golfer->GetCurrentOutfitLoadout());
    CharacterCreatorDraftOutfit = CharacterCreatorOpeningOutfit;
    CharacterCreatorOpeningCustomization =
        Golfer->GetCurrentFullCharacterCustomization();
    CharacterCreatorOpeningCustomization.Body = CharacterCreatorOpeningBody;
    CharacterCreatorOpeningCustomization.ThrowStyle = CharacterCreatorOpeningThrowStyle;
    CharacterCreatorOpeningCustomization.Identity.Handedness =
        CharacterCreatorOpeningHandedness;
    CharacterCreatorOpeningCustomization.Outfit = CharacterCreatorOpeningOutfit;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(
        CharacterCreatorOpeningCustomization);
    CharacterCreatorDraftCustomization = CharacterCreatorOpeningCustomization;
    SyncLegacyCreatorDraftsFromFull();
    bMetaHumanBackendAvailabilityChecked = false;
    bMetaHumanBackendAvailable = false;
    MetaHumanBackendAvailabilityStatus.Reset();

    const bool bOpenedControlsMenuHere = !bControlsMenuOpen;

    TSubclassOf<UDiscGolfCharacterCreatorWidget> EffectiveWidgetClass = CharacterCreatorWidgetClass;
    if (!EffectiveWidgetClass)
    {
        // A later empty Widget Blueprint can supply art without replacing the
        // functional native Slate tree. Missing optional content stays silent.
        const FString WidgetPackage = TEXT("/Game/DiscGolf/UI/WBP_DG_CharacterCreator");
        if (FPackageName::DoesPackageExist(WidgetPackage))
        {
            EffectiveWidgetClass = LoadClass<UDiscGolfCharacterCreatorWidget>(
                nullptr,
                TEXT("/Game/DiscGolf/UI/WBP_DG_CharacterCreator.WBP_DG_CharacterCreator_C"));
        }
    }
    if (!EffectiveWidgetClass)
    {
        EffectiveWidgetClass = UDiscGolfCharacterCreatorWidget::StaticClass();
    }

    CharacterCreatorWidget = CreateWidget<UDiscGolfCharacterCreatorWidget>(this, EffectiveWidgetClass);
    if (!CharacterCreatorWidget)
    {
        ControlsStatusText = TEXT("Character creator UI could not be created; active profile was retained.");
        CharacterCreatorStatusText = ControlsStatusText;
        return false;
    }

    CharacterCreatorWidget->InitializeFullCreator(
        this, CharacterCreatorOpeningCustomization);
    bCharacterCreatorPreviousMouseCursor = bShowMouseCursor;
    const TWeakObjectPtr<AActor> PreviousViewTarget = GetViewTarget();
    if (!Golfer->BeginCharacterCreatorPreview())
    {
        CharacterCreatorWidget = nullptr;
        ControlsStatusText = TEXT(
            "Character creator could not enter its verified presentation context; no UI or input state was changed.");
        CharacterCreatorStatusText = ControlsStatusText;
        return false;
    }

    // View-target ownership is a creator camera mutation, so it follows the
    // verified CharacterCreator policy gate. Refresh the camera once after the
    // target switch for a deterministic first frame, including paused entry.
    SetViewTarget(Golfer);
    if (PlayerCameraManager)
    {
        PlayerCameraManager->UpdateCamera(0.0f);
    }
    if (bOpenedControlsMenuHere)
    {
        OpenControlsMenu();
    }
    if (!CharacterCreatorWidget->AddToPlayerScreen(1000))
    {
        const bool bPreviewClosed =
            Golfer->EndCharacterCreatorPreview(true);
        if (!bPreviewClosed)
        {
            // Creator remains the last verified state. Retain both the session
            // marker and initialized widget so Open can retry only attachment,
            // while Close/Cancel can retry the safe gameplay-policy exit.
            bCharacterCreatorOpen = true;
            ControlsStatusText = TEXT(
                "Character creator screen attach failed and its safe gameplay exit also failed; the recoverable creator session remains active. Retry Open or use Cancel.");
            CharacterCreatorStatusText = ControlsStatusText;
            return false;
        }
        if (bPreviewClosed && PreviousViewTarget.IsValid())
        {
            SetViewTarget(PreviousViewTarget.Get());
            if (PlayerCameraManager)
            {
                PlayerCameraManager->UpdateCamera(0.0f);
            }
        }
        CharacterCreatorWidget = nullptr;
        ControlsStatusText = TEXT(
            "Character creator UI could not join the local player screen; active profile was retained.");
        CharacterCreatorStatusText = ControlsStatusText;
        if (bOpenedControlsMenuHere)
        {
            CloseControlsMenu();
        }
        return false;
    }

    bCharacterCreatorOpen = true;
    CharacterCreatorStatusText = TEXT("Live preview active. Apply saves to the existing local profile; Cancel restores the opening values.");
    CommitCharacterCreatorInputMode();
    return true;
}

bool ADiscGolfTourPlayerController::PreviewCharacterCreatorDraft(
    const FDGBodyProfile& Body,
    const FDGThrowStyle& ThrowStyle,
    EDGHandedness Handedness)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }

    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    if (!Golfer || !Golfer->IsCharacterProfileChangeSafe())
    {
        CharacterCreatorStatusText = TEXT("Preview blocked until the active gameplay transition finishes.");
        return false;
    }

    FDGBodyProfile SafeBody = Body;
    FDGThrowStyle SafeThrowStyle = ThrowStyle;
    EDGHandedness SafeHandedness = Handedness;
    SanitizeCreatorProfile(SafeBody, SafeThrowStyle, SafeHandedness);
    FDGBodyProfile PreviousBody;
    FDGThrowStyle PreviousStyle;
    EDGHandedness PreviousHandedness = EDGHandedness::Right;
    const bool bHadPrevious = Golfer->GetCharacterCreatorProfile(
        PreviousBody, PreviousStyle, PreviousHandedness);
    const FDGOutfitLoadout PreviousOutfit = Golfer->GetCurrentOutfitLoadout();
    if (!Golfer->PreviewCharacterCreatorProfile(SafeBody, SafeThrowStyle, SafeHandedness))
    {
        CharacterCreatorStatusText = TEXT("The current pawn rejected that preview; the last valid profile remains active.");
        return false;
    }

    FString OutfitStatus;
    if (!Golfer->ApplyOutfitLoadoutTransactionally(
            CharacterCreatorDraftOutfit, false, OutfitStatus))
    {
        if (bHadPrevious)
        {
            Golfer->PreviewCharacterCreatorProfile(
                PreviousBody, PreviousStyle, PreviousHandedness);
            FString RollbackStatus;
            Golfer->ApplyOutfitLoadoutTransactionally(
                PreviousOutfit, true, RollbackStatus);
        }
        CharacterCreatorStatusText = OutfitStatus.IsEmpty()
            ? TEXT("The current outfit is incompatible with that body preview; the last valid preview remains active.")
            : OutfitStatus;
        return false;
    }

    CharacterCreatorStatusText = SafeHandedness == EDGHandedness::Left
        ? TEXT("Left handed saved-data preview. Animated LHBH is unavailable; gameplay uses the existing non-animated fallback.")
        : TEXT("Live right-handed profile preview applied to the existing player pawn.");
    CharacterCreatorDraftCustomization =
        Golfer->GetCurrentFullCharacterCustomization();
    SyncLegacyCreatorDraftsFromFull();
    return true;
}

void ADiscGolfTourPlayerController::SyncLegacyCreatorDraftsFromFull()
{
    CharacterCreatorDraftCustomization.Outfit =
        DiscGolfOutfitRuntime::NormalizeForPersistence(
            CharacterCreatorDraftCustomization.Outfit);
    CharacterCreatorDraftOutfit = CharacterCreatorDraftCustomization.Outfit;
}

bool ADiscGolfTourPlayerController::PreviewFullCharacterCreatorDraft(
    const FDGFullCharacterCustomization& CharacterCustomization)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    if (!Golfer || !Golfer->IsCharacterProfileChangeSafe())
    {
        CharacterCreatorStatusText =
            TEXT("Preview blocked until the active gameplay transition finishes.");
        return false;
    }

    FDGFullCharacterCustomization Candidate = CharacterCustomization;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Candidate);
    FString Status;
    if (!Golfer->PreviewFullCharacterCustomization(Candidate, Status))
    {
        CharacterCreatorStatusText = Status.IsEmpty()
            ? TEXT("The full-character draft was rejected; the last valid preview remains active.")
            : Status;
        return false;
    }
    CharacterCreatorDraftCustomization =
        Golfer->GetCurrentFullCharacterCustomization();
    SyncLegacyCreatorDraftsFromFull();
    CharacterCreatorStatusText = Status.IsEmpty()
        ? (CharacterCreatorDraftCustomization.Identity.Handedness == EDGHandedness::Left
            ? TEXT("Full left-handed identity preview applied. Animated LHBH remains unavailable.")
            : TEXT("Full character draft previewed on the existing possessed pawn."))
        : Status;
    return true;
}

bool ADiscGolfTourPlayerController::ResolveCharacterCreatorPreset(
    FName PresetId,
    FDGBodyProfile& OutBody,
    FDGThrowStyle& OutThrowStyle,
    EDGHandedness& OutHandedness) const
{
    const TCHAR* AssetPath = nullptr;
    if (PresetId == FName(TEXT("Baseline")) || PresetId == FName(TEXT("Default")))
    {
        AssetPath = TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter");
    }
    else if (PresetId == FName(TEXT("ShortCompact")))
    {
#if DG_WITH_DEVELOPMENT_CONTENT
        AssetPath = TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact");
#endif
    }
    else if (PresetId == FName(TEXT("TallLongArms")))
    {
#if DG_WITH_DEVELOPMENT_CONTENT
        AssetPath = TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms");
#endif
    }

    const UDiscGolfCharacterProfile* Profile = AssetPath
        ? LoadObject<UDiscGolfCharacterProfile>(nullptr, AssetPath)
        : nullptr;
    if (!Profile)
    {
        return false;
    }

    OutBody = Profile->Body;
    OutThrowStyle = Profile->ThrowStyle;
    OutHandedness = Profile->Handedness;
    SanitizeCreatorProfile(OutBody, OutThrowStyle, OutHandedness);
    return true;
}

bool ADiscGolfTourPlayerController::LoadCharacterCreatorPreset(
    FName PresetId,
    FDGBodyProfile& OutBody,
    FDGThrowStyle& OutThrowStyle,
    EDGHandedness& OutHandedness)
{
    if (!bCharacterCreatorOpen
        || !ResolveCharacterCreatorPreset(PresetId, OutBody, OutThrowStyle, OutHandedness))
    {
        CharacterCreatorStatusText = TEXT("Requested profile preset is unavailable; the current draft was retained.");
        return false;
    }

    if (!PreviewCharacterCreatorDraft(OutBody, OutThrowStyle, OutHandedness))
    {
        return false;
    }
    CharacterCreatorStatusText = FString::Printf(
        TEXT("%s loaded into the live draft. Apply to save or Cancel to restore the opening profile."),
        *PresetId.ToString());
    return true;
}

bool ADiscGolfTourPlayerController::ResetCharacterCreatorDraft(
    FDGBodyProfile& OutBody,
    FDGThrowStyle& OutThrowStyle,
    EDGHandedness& OutHandedness)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }

    OutBody = CharacterCreatorOpeningBody;
    OutThrowStyle = CharacterCreatorOpeningThrowStyle;
    OutHandedness = CharacterCreatorOpeningHandedness;
    if (!PreviewCharacterCreatorDraft(OutBody, OutThrowStyle, OutHandedness))
    {
        return false;
    }
    CharacterCreatorStatusText = TEXT("Draft reset to the profile that was active when the creator opened.");
    return true;
}

bool ADiscGolfTourPlayerController::ResetCharacterCreatorCurrentTab(
    int32 TabIndex,
    FDGFullCharacterCustomization& OutDraft)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }
    FDGFullCharacterCustomization Candidate = CharacterCreatorDraftCustomization;
    const FDGFullCharacterCustomization Defaults =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    switch (FMath::Clamp(TabIndex, 0, 6))
    {
        case 0: Candidate.Identity = Defaults.Identity; break;
        case 1:
            Candidate.Body = Defaults.Body;
            Candidate.BodyBuild = Defaults.BodyBuild;
            break;
        case 2: Candidate.Face = Defaults.Face; break;
        case 3: Candidate.Hair = Defaults.Hair; break;
        case 4: Candidate.Appearance = Defaults.Appearance; break;
        case 5: Candidate.ThrowStyle = Defaults.ThrowStyle; break;
        case 6: Candidate.Outfit = Defaults.Outfit; break;
        default: return false;
    }
    if (!PreviewFullCharacterCreatorDraft(Candidate))
    {
        return false;
    }
    OutDraft = CharacterCreatorDraftCustomization;
    CharacterCreatorStatusText = TEXT("Current tab reset to schema defaults. Apply to save or Cancel to restore the opening character.");
    return true;
}

bool ADiscGolfTourPlayerController::ResetCharacterCreatorAll(
    FDGFullCharacterCustomization& OutDraft)
{
    if (!bCharacterCreatorOpen
        || !PreviewFullCharacterCreatorDraft(
            DiscGolfFullCharacterRuntime::MakeDefaultCustomization()))
    {
        return false;
    }
    OutDraft = CharacterCreatorDraftCustomization;
    CharacterCreatorStatusText = TEXT("All seven categories reset to schema defaults. Apply to save or Cancel to restore.");
    return true;
}

bool ADiscGolfTourPlayerController::RandomizeCharacterCreatorDraft(
    const FDiscGolfCharacterRandomizeLocks& Locks,
    FDGFullCharacterCustomization& OutDraft)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }
    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    if (!Golfer)
    {
        return false;
    }
    FRandomStream Random(static_cast<int32>(FPlatformTime::Cycles()));
    const FDGFullCharacterCustomization Candidate =
        DiscGolfFullCharacterRuntime::Randomize(
            CharacterCreatorDraftCustomization,
            Locks,
            Golfer->GetCosmeticCatalog(),
            Golfer->GetOutfitCatalog(),
            Random);
    if (!PreviewFullCharacterCreatorDraft(Candidate))
    {
        return false;
    }
    OutDraft = CharacterCreatorDraftCustomization;
    CharacterCreatorStatusText = TEXT("Valid catalog-backed character randomized; locked categories were preserved.");
    return true;
}

bool ADiscGolfTourPlayerController::ApplyCharacterCreatorFacePreset(
    FName PresetId,
    FDGFullCharacterCustomization& OutDraft)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }
    FDGFullCharacterCustomization Candidate = CharacterCreatorDraftCustomization;
    if (!DiscGolfFullCharacterRuntime::ApplyFacePreset(PresetId, Candidate.Face)
        || !PreviewFullCharacterCreatorDraft(Candidate))
    {
        CharacterCreatorStatusText = TEXT("Requested face preset is unavailable; the current face was retained.");
        return false;
    }
    OutDraft = CharacterCreatorDraftCustomization;
    CharacterCreatorStatusText = FString::Printf(
        TEXT("%s face preset applied; individual sliders remain editable."),
        *PresetId.ToString());
    return true;
}

bool ADiscGolfTourPlayerController::SelectCharacterCreatorCosmetic(
    EDGCosmeticKind Kind,
    FName ItemId,
    FDGFullCharacterCustomization& OutDraft)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }
    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    FDGFullCharacterCustomization Candidate = CharacterCreatorDraftCustomization;
    FString Status;
    if (!Golfer || !DiscGolfFullCharacterRuntime::SetCosmeticSelection(
            Candidate, Kind, ItemId, Golfer->GetCosmeticCatalog(), Status)
        || !PreviewFullCharacterCreatorDraft(Candidate))
    {
        CharacterCreatorStatusText = Status.IsEmpty()
            ? TEXT("That cosmetic could not be previewed; the last valid selection remains active.")
            : Status;
        return false;
    }
    OutDraft = CharacterCreatorDraftCustomization;
    CharacterCreatorStatusText = Status;
    return true;
}

TArray<FDiscGolfCosmeticOption>
ADiscGolfTourPlayerController::GetCharacterCreatorCosmeticOptions(
    EDGCosmeticKind Kind) const
{
    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    // RebuildWidget queries immutable options during AddToPlayerScreen, just
    // before OpenCharacterCreator flips the public open bit. The widget ptr is
    // already assigned at that point; mutation APIs remain strictly gated.
    if ((!bCharacterCreatorOpen && !CharacterCreatorWidget) || !Golfer)
    {
        return {};
    }
    return DiscGolfFullCharacterRuntime::GetCosmeticOptions(
        Golfer->GetCosmeticCatalog(),
        Kind,
        CharacterCreatorDraftCustomization.Body.HeightCm);
}

bool ADiscGolfTourPlayerController::SelectCharacterCreatorBackend(
    FName BackendId,
    FDGFullCharacterCustomization& OutDraft)
{
    OutDraft = CharacterCreatorDraftCustomization;
    if (!bCharacterCreatorOpen)
    {
        return false;
    }

    const FName DGMasterId(DiscGolfAvatarBackendRuntime::DGMasterBackendId);
    const FName MetaHumanId(
        DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId);
    if (BackendId != DGMasterId && BackendId != MetaHumanId)
    {
        CharacterCreatorStatusText =
            TEXT("Unknown visual backend; the current draft was retained.");
        return false;
    }

    if (BackendId == MetaHumanId
        && !IsCharacterCreatorMetaHumanBackendAvailable())
    {
        CharacterCreatorStatusText = GetCharacterCreatorBackendStatusText();
        return false;
    }

    FDGFullCharacterCustomization Candidate =
        CharacterCreatorDraftCustomization;
    Candidate.AvatarBackendId = BackendId;
    if (!PreviewFullCharacterCreatorDraft(Candidate))
    {
        return false;
    }
    OutDraft = CharacterCreatorDraftCustomization;
    CharacterCreatorStatusText = BackendId == MetaHumanId
        ? TEXT("Verified realistic character preview selected. Proxy-only face, body, hair, appearance, and outfit values remain preserved for DGMaster fallback.")
        : TEXT("DGMaster proxy preview selected. All seven proxy customization tabs are active.");
    return true;
}

bool ADiscGolfTourPlayerController::
IsCharacterCreatorMetaHumanBackendAvailable() const
{
    CacheCharacterCreatorMetaHumanAvailability();
    return bMetaHumanBackendAvailable;
}

FString ADiscGolfTourPlayerController::
GetCharacterCreatorBackendStatusText() const
{
    CacheCharacterCreatorMetaHumanAvailability();
    return MetaHumanBackendAvailabilityStatus;
}

void ADiscGolfTourPlayerController::
CacheCharacterCreatorMetaHumanAvailability() const
{
    if (bMetaHumanBackendAvailabilityChecked)
    {
        return;
    }
    bMetaHumanBackendAvailabilityChecked = true;
    const UDiscGolfAvatarBackendProfile* Profile =
        LoadObject<UDiscGolfAvatarBackendProfile>(
            nullptr,
            DiscGolfAvatarBackendRuntime::MetaHumanDefaultProfileObjectPath);
    const FDiscGolfAvatarBackendResolution Resolution =
        DiscGolfAvatarBackendRuntime::ResolveBackend(
            DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId,
            Profile);
    bMetaHumanBackendAvailable = Resolution.bMetaHumanAttemptAllowed;
    MetaHumanBackendAvailabilityStatus = Resolution.bMetaHumanAttemptAllowed
        ? TEXT("Realistic character backend is available; runtime preview still fails closed unless actor and retarget verification succeed.")
        : TEXT("Realistic character backend is unavailable; DGMaster fallback remains active.");
}

bool ADiscGolfTourPlayerController::ApplyCharacterCreatorDraft(
    const FDGBodyProfile& Body,
    const FDGThrowStyle& ThrowStyle,
    EDGHandedness Handedness)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }

    FDGBodyProfile SafeBody = Body;
    FDGThrowStyle SafeThrowStyle = ThrowStyle;
    EDGHandedness SafeHandedness = Handedness;
    SanitizeCreatorProfile(SafeBody, SafeThrowStyle, SafeHandedness);
    if (!PreviewCharacterCreatorDraft(SafeBody, SafeThrowStyle, SafeHandedness))
    {
        return false;
    }

    UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    const FDiscGolfCharacterProfileSaveData Existing = Instance
        ? Instance->GetCharacterProfile() : FDiscGolfCharacterProfileSaveData();
    const FDiscGolfCharacterProfileSaveData SaveData =
        FDiscGolfCharacterProfileSaveData::FromFramework(
            SafeBody,
            SafeThrowStyle,
            SafeHandedness,
            Existing.ToBodyBuildProfile());
    if (!Instance || !Instance->UpdateCharacterProfileAndOutfit(
            SaveData, CharacterCreatorDraftOutfit))
    {
        CharacterCreatorStatusText = TEXT("Profile save failed. The creator remains open and no saved profile was replaced.");
        return false;
    }

    CharacterCreatorStatusText = SafeHandedness == EDGHandedness::Left
        ? TEXT("Profile saved. Animated LHBH remains deferred; the documented gameplay fallback is active.")
        : TEXT("Profile saved to the existing local player profile.");
    ControlsStatusText = CharacterCreatorStatusText;
    if (!CloseCharacterCreator(false))
    {
        CharacterCreatorStatusText = TEXT(
            "Profile saved, but the creator remains open because the verified gameplay presentation could not be restored.");
        ControlsStatusText = CharacterCreatorStatusText;
        return false;
    }
    return true;
}

bool ADiscGolfTourPlayerController::ApplyFullCharacterCreatorDraft(
    const FDGFullCharacterCustomization& CharacterCustomization)
{
    if (!bCharacterCreatorOpen
        || !PreviewFullCharacterCreatorDraft(CharacterCustomization))
    {
        return false;
    }
    UDiscGolfTourGameInstance* Instance =
        Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    if (!Instance || !Instance->UpdateFullCharacterCustomization(
            CharacterCreatorDraftCustomization))
    {
        CharacterCreatorStatusText = TEXT("Full-character save failed. The creator remains open and the saved profile was not replaced.");
        return false;
    }
    CharacterCreatorStatusText =
        CharacterCreatorDraftCustomization.Identity.Handedness == EDGHandedness::Left
        ? TEXT("Complete character saved. Animated LHBH remains deferred; the gameplay fallback is active.")
        : TEXT("Complete character saved to the existing local player profile.");
    ControlsStatusText = CharacterCreatorStatusText;
    if (!CloseCharacterCreator(false))
    {
        CharacterCreatorStatusText = TEXT(
            "Complete character saved, but the creator remains open because the verified gameplay presentation could not be restored.");
        ControlsStatusText = CharacterCreatorStatusText;
        return false;
    }
    return true;
}

void ADiscGolfTourPlayerController::CancelCharacterCreator()
{
    if (!bCharacterCreatorOpen)
    {
        return;
    }

    ControlsStatusText = TEXT("Character changes cancelled; the opening profile was restored.");
    CloseCharacterCreator(true);
}

void ADiscGolfTourPlayerController::RotateCharacterCreatorPreview(float DeltaYawDegrees)
{
    if (!bCharacterCreatorOpen || !FMath::IsFinite(DeltaYawDegrees))
    {
        return;
    }
    if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn()))
    {
        Golfer->RotateCharacterCreatorPreview(FMath::Clamp(DeltaYawDegrees, -45.0f, 45.0f));
    }
}

void ADiscGolfTourPlayerController::ZoomCharacterCreatorPreview(float DeltaArmLength)
{
    if (!bCharacterCreatorOpen || !FMath::IsFinite(DeltaArmLength))
    {
        return;
    }
    if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn()))
    {
        Golfer->ZoomCharacterCreatorPreview(
            FMath::Clamp(DeltaArmLength, -100.0f, 100.0f));
    }
}

bool ADiscGolfTourPlayerController::PreviewCharacterCreatorOutfitSelection(
    EDGOutfitSlot Slot,
    FName ItemId,
    FName VariantId,
    FDGOutfitLoadout& OutDraft)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }

    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    if (!Golfer || !Golfer->IsCharacterProfileChangeSafe()
        || !Golfer->GetRuntimeCharacterProfile())
    {
        CharacterCreatorStatusText = TEXT("Outfit preview is blocked until gameplay returns to a safe state.");
        return false;
    }

    FDGOutfitLoadout Candidate = CharacterCreatorDraftOutfit;
    FString SelectionStatus;
    if (!DiscGolfOutfitRuntime::SetSlotSelection(
            Candidate,
            Slot,
            ItemId,
            VariantId,
            Golfer->GetOutfitCatalog(),
            Golfer->GetRuntimeCharacterProfile()->Body,
            SelectionStatus))
    {
        CharacterCreatorStatusText = SelectionStatus;
        return false;
    }
    FDGFullCharacterCustomization FullCandidate =
        CharacterCreatorDraftCustomization;
    FullCandidate.Outfit = Candidate;
    FString ApplyStatus;
    if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
            FullCandidate, false, ApplyStatus))
    {
        CharacterCreatorStatusText = ApplyStatus;
        return false;
    }

    CharacterCreatorDraftOutfit = Golfer->GetCurrentOutfitLoadout();
    CharacterCreatorDraftCustomization = Golfer->GetCurrentFullCharacterCustomization();
    SyncLegacyCreatorDraftsFromFull();
    OutDraft = CharacterCreatorDraftOutfit;
    CharacterCreatorStatusText = ApplyStatus.IsEmpty()
        ? SelectionStatus : ApplyStatus;
    return true;
}

bool ADiscGolfTourPlayerController::ResetCharacterCreatorOutfit(
    FDGOutfitLoadout& OutDraft)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    FString Status;
    const FDGOutfitLoadout Empty;
    FDGFullCharacterCustomization FullCandidate =
        CharacterCreatorDraftCustomization;
    FullCandidate.Outfit = Empty;
    if (!Golfer || !Golfer->ApplyFullCharacterCustomizationTransactionally(
            FullCandidate, false, Status))
    {
        CharacterCreatorStatusText = Status;
        return false;
    }
    CharacterCreatorDraftOutfit = Golfer->GetCurrentOutfitLoadout();
    CharacterCreatorDraftCustomization = Golfer->GetCurrentFullCharacterCustomization();
    SyncLegacyCreatorDraftsFromFull();
    OutDraft = CharacterCreatorDraftOutfit;
    CharacterCreatorStatusText = TEXT("Outfit draft reset to None in every slot. Apply to save or Cancel to restore.");
    return true;
}

bool ADiscGolfTourPlayerController::RandomizeCharacterCreatorOutfit(
    FDGOutfitLoadout& OutDraft)
{
    if (!bCharacterCreatorOpen)
    {
        return false;
    }
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    if (!Golfer || !Golfer->GetRuntimeCharacterProfile())
    {
        return false;
    }

    FDGOutfitLoadout Candidate;
    FRandomStream Random(static_cast<int32>(FPlatformTime::Cycles()));
    for (EDGOutfitSlot Slot : DiscGolfOutfitRuntime::GetOrderedSlots())
    {
        const TArray<FDiscGolfOutfitOption> Options =
            DiscGolfOutfitRuntime::GetOptionsForSlot(
                Golfer->GetOutfitCatalog(), Slot, Golfer->GetRuntimeCharacterProfile()->Body);
        TArray<const FDiscGolfOutfitOption*> Compatible;
        for (const FDiscGolfOutfitOption& Option : Options)
        {
            if (Option.bCompatible && !Option.VariantIds.IsEmpty())
            {
                Compatible.Add(&Option);
            }
        }
        if (Compatible.IsEmpty())
        {
            continue;
        }

        const FDiscGolfOutfitOption* Selected = Compatible[Random.RandRange(0, Compatible.Num() - 1)];
        const FName VariantId = Selected->VariantIds[Random.RandRange(0, Selected->VariantIds.Num() - 1)];
        FString IgnoredStatus;
        DiscGolfOutfitRuntime::SetSlotSelection(
            Candidate,
            Slot,
            Selected->ItemId,
            VariantId,
            Golfer->GetOutfitCatalog(),
            Golfer->GetRuntimeCharacterProfile()->Body,
            IgnoredStatus);
    }

    FDGFullCharacterCustomization FullCandidate =
        CharacterCreatorDraftCustomization;
    FullCandidate.Outfit = Candidate;
    FString Status;
    if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
            FullCandidate, false, Status))
    {
        CharacterCreatorStatusText = Status;
        return false;
    }
    CharacterCreatorDraftOutfit = Golfer->GetCurrentOutfitLoadout();
    CharacterCreatorDraftCustomization = Golfer->GetCurrentFullCharacterCustomization();
    SyncLegacyCreatorDraftsFromFull();
    OutDraft = CharacterCreatorDraftOutfit;
    CharacterCreatorStatusText = TEXT("Compatible proxy outfit randomized. Apply to save or Cancel to restore.");
    return true;
}

TArray<FDiscGolfOutfitOption> ADiscGolfTourPlayerController::GetCharacterCreatorOutfitOptions(
    EDGOutfitSlot Slot) const
{
    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn());
    if (!bCharacterCreatorOpen || !Golfer || !Golfer->GetRuntimeCharacterProfile())
    {
        return {};
    }
    return DiscGolfOutfitRuntime::GetOptionsForSlot(
        Golfer->GetOutfitCatalog(), Slot, Golfer->GetRuntimeCharacterProfile()->Body);
}

bool ADiscGolfTourPlayerController::PrepareCharacterCreatorForSession6VisualEvidence(
    EDGOutfitSlot Slot)
{
#if !DG_WITH_DEVELOPMENT_CONTENT
    return false;
#else
    if (!FParse::Param(FCommandLine::Get(), TEXT("Session6OutfitVisualCapture")))
    {
        return false;
    }
    if (!bCharacterCreatorOpen || !CharacterCreatorWidget)
    {
        return false;
    }

    CharacterCreatorWidget->PrepareSession6VisualOutfitEvidence(
        Slot, CharacterCreatorDraftOutfit);
    return true;
#endif
}

bool ADiscGolfTourPlayerController::PrepareCharacterCreatorForSession7VisualEvidence(
    int32 TabIndex,
    const FDGFullCharacterCustomization& Draft)
{
#if !DG_WITH_DEVELOPMENT_CONTENT
    return false;
#else
    if (!FParse::Param(FCommandLine::Get(), TEXT("Session7FullCharacterVisualCapture"))
        || !bCharacterCreatorOpen || !CharacterCreatorWidget)
    {
        return false;
    }
    if (!PreviewFullCharacterCreatorDraft(Draft))
    {
        return false;
    }
    CharacterCreatorWidget->PrepareSession7VisualEvidence(
        FMath::Clamp(TabIndex, 0, 6), CharacterCreatorDraftCustomization);
    return true;
#endif
}

int32 ADiscGolfTourPlayerController::GetCharacterCreatorActiveTabIndex() const
{
    return CharacterCreatorWidget
        ? CharacterCreatorWidget->GetActiveCreatorTabIndex() : INDEX_NONE;
}

void ADiscGolfTourPlayerController::GetSession7CharacterCreatorVisibleControlIds(
    TArray<FString>& OutControlIds) const
{
    OutControlIds.Reset();
    if (bCharacterCreatorOpen && CharacterCreatorWidget)
    {
        CharacterCreatorWidget->GetSession7VisibleControlIds(OutControlIds);
    }
}

void ADiscGolfTourPlayerController::CommitCharacterCreatorInputMode()
{
    if (!CharacterCreatorWidget)
    {
        return;
    }
    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    const TSharedPtr<SWidget> InitialFocusWidget =
        CharacterCreatorWidget->GetInitialFocusWidget();
    InputMode.SetWidgetToFocus(InitialFocusWidget.IsValid()
        ? InitialFocusWidget.ToSharedRef()
        : CharacterCreatorWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    SetInputMode(InputMode);
    if (InitialFocusWidget.IsValid()
        && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetKeyboardFocus(
            InitialFocusWidget, EFocusCause::SetDirectly);
    }
    FlushPressedKeys();
}

bool ADiscGolfTourPlayerController::CloseCharacterCreator(
    bool bRestoreOpeningProfile)
{
    if (!bCharacterCreatorOpen)
    {
        return true;
    }

    ADiscGolferPawn* const Golfer = Cast<ADiscGolferPawn>(GetPawn());
    if (!Golfer)
    {
        ControlsStatusText = TEXT(
            "Character creator cannot close without its possessed golfer; the UI remains open.");
        CharacterCreatorStatusText = ControlsStatusText;
        return false;
    }
    if (bRestoreOpeningProfile)
    {
        FString FullRestoreStatus;
        if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
                CharacterCreatorOpeningCustomization,
                true,
                FullRestoreStatus,
                true))
        {
            ControlsStatusText = TEXT("Cancel is waiting for a safe full-character transition; the creator remains open.");
            if (!FullRestoreStatus.IsEmpty())
            {
                ControlsStatusText += TEXT(" ") + FullRestoreStatus;
            }
            CharacterCreatorStatusText = ControlsStatusText;
            return false;
        }

        // Preserve the accepted Session 4/6 compatibility seam after the
        // complete snapshot has been restored. These values are slices of
        // CharacterCreatorOpeningCustomization, not a competing draft.
        FDGBodyProfile PreviousBody;
        FDGThrowStyle PreviousStyle;
        EDGHandedness PreviousHandedness = EDGHandedness::Right;
        const bool bHadPrevious = Golfer->GetCharacterCreatorProfile(
            PreviousBody, PreviousStyle, PreviousHandedness);
        const FDGOutfitLoadout PreviousOutfit = Golfer->GetCurrentOutfitLoadout();

        FString OutfitStatus;
        if (!Golfer->PreviewCharacterCreatorProfile(
                CharacterCreatorOpeningBody,
                CharacterCreatorOpeningThrowStyle,
                CharacterCreatorOpeningHandedness,
                true)
            || !Golfer->ApplyOutfitLoadoutTransactionally(
                CharacterCreatorOpeningOutfit, false, OutfitStatus))
        {
            if (bHadPrevious)
            {
                Golfer->PreviewCharacterCreatorProfile(
                    PreviousBody, PreviousStyle, PreviousHandedness);
                FString RollbackStatus;
                Golfer->ApplyOutfitLoadoutTransactionally(
                    PreviousOutfit, true, RollbackStatus);
            }
            ControlsStatusText = TEXT("Cancel is waiting for a safe profile transition; the creator remains open.");
            if (!OutfitStatus.IsEmpty())
            {
                ControlsStatusText += TEXT(" ") + OutfitStatus;
            }
            CharacterCreatorStatusText = ControlsStatusText;
            return false;
        }
    }
    if (!Golfer->EndCharacterCreatorPreview(true))
    {
        ControlsStatusText = bRestoreOpeningProfile
            ? TEXT("Opening character restored, but Cancel is waiting for the verified gameplay presentation; the creator remains open.")
            : TEXT("Saved character retained, but Apply is waiting for the verified gameplay presentation; the creator remains open.");
        CharacterCreatorStatusText = ControlsStatusText;
        return false;
    }

    if (CharacterCreatorWidget)
    {
        CharacterCreatorWidget->RemoveFromParent();
        CharacterCreatorWidget = nullptr;
    }
    bCharacterCreatorOpen = false;
    bShowMouseCursor = bCharacterCreatorPreviousMouseCursor;

    if (bControlsMenuOpen)
    {
        CloseControlsMenu();
    }
    RefreshRoundFlowPresentation();
    if (bHasRoundFlowSnapshot
        && ActiveRoundFlowSnapshot.Screen != EDGRoundFlowScreen::Hidden)
    {
        ApplyRoundFlowInputMode();
    }
    else
    {
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        FlushPressedKeys();
    }
    return true;
}

void ADiscGolfTourPlayerController::OpenControlsMenu()
{
    if (bControlsMenuOpen)
    {
        return;
    }
    EnsureGameplayInputReady();
    if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(GetPawn()))
    {
        // Opening settings is a new input authority. Retire any pre-menu
        // animation transaction so an old montage notify cannot launch after
        // the world resumes without a fresh throw press.
        Golfer->CancelAnimatedThrow();
        Golfer->CancelThrowPresentation();
    }
    bControlsMenuOpen = true;
    bWaitingForControlBinding = false;
    bWasPausedBeforeControlsMenu = GetWorld() && GetWorld()->IsPaused();
    bControlsMenuSuspendedPlayabilityMonitor = false;
    if (ADiscGolfTourGameMode* GameMode = GetWorld()
            ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr)
    {
        if (UDiscGolfPlayabilityMonitorComponent* Monitor = GameMode->GetPlayabilityMonitor();
            Monitor && Monitor->IsMonitoring())
        {
            Monitor->SuspendMonitoring(TEXT("ControlsMenu"));
            bControlsMenuSuspendedPlayabilityMonitor = true;
        }
    }
    ControlsStatusText = TEXT("Player settings. Tab / Shoulders switches to controls.");
    SetGameplayContextEnabled(false);
    if (!bWasPausedBeforeControlsMenu)
    {
        SetPause(true);
    }
    ApplyRoundFlowInputMode();
}

FDiscGolfPlayerSettings ADiscGolfTourPlayerController::GetPlayerSettings() const
{
    const UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    return Instance ? Instance->GetPlayerSettings() : FDiscGolfPlayerSettings();
}

FDiscGolfPlayerSettings ADiscGolfTourPlayerController::GetCurrentPlayerSettings() const
{
    return GetPlayerSettings();
}

void ADiscGolfTourPlayerController::CommitPlayerSettings(const FDiscGolfPlayerSettings& Settings)
{
    if (UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance()))
    {
        Instance->UpdatePlayerSettings(Settings);
        RefreshRuntimeInputFromPlayerSettings();
    }
}

void ADiscGolfTourPlayerController::ToggleMenuPage()
{
    bSettingsPage = !bSettingsPage;
    bWaitingForControlBinding = false;
    ControlsStatusText = bSettingsPage
        ? TEXT("Player settings. Left / Right adjusts the selected option.")
        : TEXT("Controls. Select a binding and press Enter / Bottom to rebind.");
}

void ADiscGolfTourPlayerController::MoveSettingsSelection(int32 Direction)
{
    SelectedSettingIndex = WrapIndex(SelectedSettingIndex + Direction, PlayerSettingRowCount);
}

void ADiscGolfTourPlayerController::AdjustSelectedSetting(int32 Direction)
{
    FDiscGolfPlayerSettings Settings = GetPlayerSettings();
    switch (SelectedSettingIndex)
    {
        case 0: Settings.GraphicsQuality = WrapIndex(Settings.GraphicsQuality + Direction, 4); break;
        case 1:
        {
            const FIntPoint Resolutions[] = {
                FIntPoint(1280, 720), FIntPoint(1920, 1080),
                FIntPoint(2560, 1440), FIntPoint(3840, 2160)
            };
            int32 Current = 0;
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Resolutions); ++Index)
            {
                if (Settings.ResolutionX == Resolutions[Index].X
                    && Settings.ResolutionY == Resolutions[Index].Y)
                {
                    Current = Index;
                    break;
                }
            }
            const FIntPoint Resolution = Resolutions[WrapIndex(
                Current + Direction, UE_ARRAY_COUNT(Resolutions))];
            Settings.ResolutionX = Resolution.X;
            Settings.ResolutionY = Resolution.Y;
            break;
        }
        case 2: Settings.WindowMode = WrapIndex(Settings.WindowMode + Direction, 3); break;
        case 3: Settings.MasterVolume += Direction * 0.05f; break;
        case 4: Settings.MusicVolume += Direction * 0.05f; break;
        case 5: Settings.EffectsVolume += Direction * 0.05f; break;
        case 6: Settings.AmbienceVolume += Direction * 0.05f; break;
        case 7: Settings.VoiceVolume += Direction * 0.05f; break;
        case 8: Settings.MouseSensitivity += Direction * 0.10f; break;
        case 9: Settings.ControllerSensitivity += Direction * 0.10f; break;
        case 10: Settings.ControllerDeadZone += Direction * 0.05f; break;
        case 11: Settings.bInvertY = !Settings.bInvertY; break;
        case 12: Settings.bSouthpawController = !Settings.bSouthpawController; break;
        case 13: Settings.CameraShakeStrength += Direction * 0.10f; break;
        case 14: Settings.bAutoFollowDisc = !Settings.bAutoFollowDisc; break;
        case 15: Settings.ReplaySpeed += Direction * 0.25f; break;
        case 16: Settings.bHudVisible = !Settings.bHudVisible; break;
        case 17: Settings.bBasketMarkerVisible = !Settings.bBasketMarkerVisible; break;
        case 18: Settings.Units = Settings.Units == EDiscGolfUnitSystem::Imperial ? EDiscGolfUnitSystem::Metric : EDiscGolfUnitSystem::Imperial; break;
        case 19: Settings.HudScale += Direction * 0.05f; break;
        case 20: Settings.TextScale += Direction * 0.05f; break;
        case 21: Settings.bReducedMotion = !Settings.bReducedMotion; break;
        case 22: Settings.bHighContrastBasketMarker = !Settings.bHighContrastBasketMarker; break;
        case 23: Settings.bHighContrastUI = !Settings.bHighContrastUI; break;
        case 24: Settings.ColorVisionMode = static_cast<EDiscGolfColorVisionMode>(WrapIndex(
            static_cast<int32>(Settings.ColorVisionMode) + Direction, 4)); break;
        case 25: Settings.TracerColorPreset = static_cast<EDiscGolfTracerColorPreset>(WrapIndex(
            static_cast<int32>(Settings.TracerColorPreset) + Direction, 4)); break;
        case 26: Settings.bSubtitles = !Settings.bSubtitles; break;
        case 27: Settings.bAimingIndicatorVisible = !Settings.bAimingIndicatorVisible; break;
        case 28: Settings.AimAssist01 += Direction * 0.05f; break;
        case 29: Settings.TimingWindowScale += Direction * 0.10f; break;
        case 30: Settings.bShotShapeGuide = !Settings.bShotShapeGuide; break;
        case 31: Settings.bOptionalFlightPreview = !Settings.bOptionalFlightPreview; break;
        default: break;
    }
    Settings.Normalize();
    CommitPlayerSettings(Settings);
    ControlsStatusText = TEXT("Setting saved to the local profile.");
}

void ADiscGolfTourPlayerController::GetSettingRows(TArray<FDiscGolfSettingRow>& OutRows) const
{
    const FDiscGolfPlayerSettings S = GetPlayerSettings();
    const TCHAR* Quality[] = {TEXT("PERFORMANCE"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC")};
    const TCHAR* Window[] = {TEXT("FULLSCREEN"), TEXT("BORDERLESS"), TEXT("WINDOWED")};
    const auto Add = [&OutRows](const TCHAR* Label, const FString& Value)
    {
        FDiscGolfSettingRow& Row = OutRows.AddDefaulted_GetRef(); Row.Label = Label; Row.Value = Value;
    };
    OutRows.Reset(PlayerSettingRowCount);
    Add(TEXT("GRAPHICS QUALITY"), Quality[FMath::Clamp(S.GraphicsQuality, 0, 3)]);
    Add(TEXT("RESOLUTION"), FString::Printf(TEXT("%d X %d"), S.ResolutionX, S.ResolutionY));
    Add(TEXT("WINDOW MODE"), Window[FMath::Clamp(S.WindowMode, 0, 2)]);
    Add(TEXT("MASTER VOLUME"), FString::Printf(TEXT("%.0f%%"), S.MasterVolume * 100.0f));
    Add(TEXT("MUSIC VOLUME"), FString::Printf(TEXT("%.0f%%"), S.MusicVolume * 100.0f));
    Add(TEXT("EFFECTS VOLUME"), FString::Printf(TEXT("%.0f%%"), S.EffectsVolume * 100.0f));
    Add(TEXT("AMBIENCE VOLUME"), FString::Printf(TEXT("%.0f%%"), S.AmbienceVolume * 100.0f));
    Add(TEXT("VOICE VOLUME"), FString::Printf(TEXT("%.0f%%"), S.VoiceVolume * 100.0f));
    Add(TEXT("MOUSE SENSITIVITY"), FString::Printf(TEXT("%.1f"), S.MouseSensitivity));
    Add(TEXT("CONTROLLER SENSITIVITY"), FString::Printf(TEXT("%.1f"), S.ControllerSensitivity));
    Add(TEXT("STICK DEAD ZONE"), FString::Printf(TEXT("%.0f%%"), S.ControllerDeadZone * 100.0f));
    Add(TEXT("INVERT Y"), S.bInvertY ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("CONTROLLER PRESET"), S.bSouthpawController ? TEXT("SOUTHPAW") : TEXT("DEFAULT"));
    Add(TEXT("CAMERA SHAKE"), FString::Printf(TEXT("%.0f%%"), S.CameraShakeStrength * 100.0f));
    Add(TEXT("AUTO-FOLLOW DISC"), S.bAutoFollowDisc ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("REPLAY SPEED"), FString::Printf(TEXT("%.2fX"), S.ReplaySpeed));
    Add(TEXT("HUD"), S.bHudVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
    Add(TEXT("BASKET MARKER"), S.bBasketMarkerVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
    Add(TEXT("UNITS"), S.Units == EDiscGolfUnitSystem::Imperial ? TEXT("FEET / IMPERIAL") : TEXT("METERS / METRIC"));
    Add(TEXT("HUD SCALE"), FString::Printf(TEXT("%.0f%%"), S.HudScale * 100.0f));
    Add(TEXT("TEXT SCALE"), FString::Printf(TEXT("%.0f%%"), S.TextScale * 100.0f));
    Add(TEXT("REDUCED MOTION"), S.bReducedMotion ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("HIGH-CONTRAST MARKER"), S.bHighContrastBasketMarker ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("HIGH-CONTRAST UI"), S.bHighContrastUI ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("COLOR VISION"), DiscGolfPlayerExperience::ColorVisionModeName(S.ColorVisionMode));
    Add(TEXT("TRACER COLOR"), DiscGolfPlayerExperience::TracerColorPresetName(S.TracerColorPreset));
    Add(TEXT("SUBTITLES"), S.bSubtitles ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("AIM INDICATOR"), S.bAimingIndicatorVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
    Add(TEXT("AIM ASSIST"), FString::Printf(TEXT("%.0f%%"), S.AimAssist01 * 100.0f));
    Add(TEXT("TIMING WINDOW"), FString::Printf(TEXT("%.0f%%"), S.TimingWindowScale * 100.0f));
    Add(TEXT("SHOT-SHAPE GUIDE"), S.bShotShapeGuide ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("OPTIONAL FLIGHT PREVIEW"), S.bOptionalFlightPreview ? TEXT("ON") : TEXT("OFF"));
}

void ADiscGolfTourPlayerController::CloseControlsMenu()
{
    if (!bControlsMenuOpen)
    {
        return;
    }

    const bool bRestoreRoundFlow = bRoundFlowSettingsOriginValid;
    const EDGRoundFlowScreen ExpectedOrigin = RoundFlowSettingsOrigin;
    bControlsMenuOpen = false;
    bWaitingForControlBinding = false;
    SetGameplayContextEnabled(!bRestoreRoundFlow);
    if (!bWasPausedBeforeControlsMenu)
    {
        SetPause(false);
    }
    if (bControlsMenuSuspendedPlayabilityMonitor)
    {
        if (ADiscGolfTourGameMode* GameMode = GetWorld()
                ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr)
        {
            if (UDiscGolfPlayabilityMonitorComponent* Monitor = GameMode->GetPlayabilityMonitor())
            {
                Monitor->ResumeMonitoring(TEXT("DiscInFlight"));
            }
        }
        bControlsMenuSuspendedPlayabilityMonitor = false;
    }

    if (bRestoreRoundFlow)
    {
        bRoundFlowSettingsOriginValid = false;
        RoundFlowSettingsOrigin = EDGRoundFlowScreen::Hidden;
        bRoundFlowWidgetCreationAttempted = false;
        RoundFlowWidgetAttemptedScreen = EDGRoundFlowScreen::Hidden;
        RefreshRoundFlowPresentation();
        if (!bHasRoundFlowSnapshot
            || ActiveRoundFlowSnapshot.Screen != ExpectedOrigin)
        {
            UE_LOG(LogDiscGolfTour, Warning,
                TEXT("Settings returned to changed round-flow state instead of its original screen."));
        }
    }
    else
    {
        bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        SetInputMode(InputMode);
        FlushPressedKeys();
    }
}

void ADiscGolfTourPlayerController::GetControlDefinitions(
    TArray<FName>& OutMappingNames,
    TArray<FString>& OutDisplayNames) const
{
    OutMappingNames.Reset();
    OutDisplayNames.Reset();
    if (!EffectiveInputConfig) return;

    TArray<const UInputAction*> Actions;
    EffectiveInputConfig->GetOrderedActions(Actions);
    for (const UInputAction* Action : Actions)
    {
        if (!Action) continue;
        const UPlayerMappableKeySettings* MappingSettings = Action->GetPlayerMappableKeySettings();
        const FName MappingName = MappingSettings && !MappingSettings->GetMappingName().IsNone()
            ? MappingSettings->GetMappingName()
            : Action->GetFName();
        const FString DisplayName = MappingSettings && !MappingSettings->DisplayName.IsEmpty()
            ? MappingSettings->DisplayName.ToString()
            : (!Action->ActionDescription.IsEmpty() ? Action->ActionDescription.ToString() : MappingName.ToString());
        OutMappingNames.Add(MappingName);
        OutDisplayNames.Add(DisplayName);
    }
}

void ADiscGolfTourPlayerController::GetSortedMappings(
    FName MappingName,
    TArray<const FPlayerKeyMapping*>& OutMappings) const
{
    OutMappings.Reset();
    const UEnhancedInputUserSettings* UserSettings = GetEnhancedInputUserSettings();
    if (!UserSettings) return;

    const TSet<FPlayerKeyMapping>& Mappings = UserSettings->FindMappingsInRow(MappingName);
    for (uint8 Slot = static_cast<uint8>(EPlayerMappableKeySlot::First);
         Slot < static_cast<uint8>(EPlayerMappableKeySlot::Max);
         ++Slot)
    {
        for (const FPlayerKeyMapping& Mapping : Mappings)
        {
            if (static_cast<uint8>(Mapping.GetSlot()) == Slot)
            {
                OutMappings.Add(&Mapping);
            }
        }
    }
}

void ADiscGolfTourPlayerController::GetControlBindingRows(TArray<FDiscGolfControlBindingRow>& OutRows) const
{
    OutRows.Reset();
    TArray<FName> MappingNames;
    TArray<FString> DisplayNames;
    GetControlDefinitions(MappingNames, DisplayNames);

    for (int32 Index = 0; Index < MappingNames.Num(); ++Index)
    {
        FDiscGolfControlBindingRow& Row = OutRows.AddDefaulted_GetRef();
        Row.DisplayName = DisplayNames.IsValidIndex(Index) ? DisplayNames[Index] : MappingNames[Index].ToString();

        TArray<const FPlayerKeyMapping*> Mappings;
        GetSortedMappings(MappingNames[Index], Mappings);
        for (const FPlayerKeyMapping* Mapping : Mappings)
        {
            FString Label = Mapping->GetCurrentKey().GetDisplayName(false).ToString();
            if (Mapping->IsCustomized()) Label += TEXT(" *");
            Row.BindingLabels.Add(Label);
        }
    }
}

void ADiscGolfTourPlayerController::MoveControlSelection(int32 Direction)
{
    TArray<FName> MappingNames;
    TArray<FString> DisplayNames;
    GetControlDefinitions(MappingNames, DisplayNames);
    if (MappingNames.IsEmpty()) return;

    SelectedControlIndex = (SelectedControlIndex + Direction + MappingNames.Num()) % MappingNames.Num();
    SelectedBindingIndex = 0;
    ControlsStatusText = TEXT("Select a binding slot, then press Enter / Bottom.");
}

void ADiscGolfTourPlayerController::MoveBindingSelection(int32 Direction)
{
    FName MappingName;
    GetSelectedPlayerMapping(MappingName);
    TArray<const FPlayerKeyMapping*> Mappings;
    GetSortedMappings(MappingName, Mappings);
    if (Mappings.IsEmpty()) return;

    SelectedBindingIndex = (SelectedBindingIndex + Direction + Mappings.Num()) % Mappings.Num();
    ControlsStatusText = TEXT("Press Enter / Bottom to change the highlighted binding.");
}

const FPlayerKeyMapping* ADiscGolfTourPlayerController::GetSelectedPlayerMapping(FName& OutMappingName) const
{
    TArray<FName> MappingNames;
    TArray<FString> DisplayNames;
    GetControlDefinitions(MappingNames, DisplayNames);
    if (!MappingNames.IsValidIndex(SelectedControlIndex)) return nullptr;

    OutMappingName = MappingNames[SelectedControlIndex];
    TArray<const FPlayerKeyMapping*> Mappings;
    GetSortedMappings(OutMappingName, Mappings);
    return Mappings.IsValidIndex(SelectedBindingIndex) ? Mappings[SelectedBindingIndex] : nullptr;
}

void ADiscGolfTourPlayerController::BeginControlBindingCapture()
{
    FName MappingName;
    if (!GetSelectedPlayerMapping(MappingName))
    {
        ControlsStatusText = TEXT("No remappable binding is available for this action.");
        return;
    }

    bWaitingForControlBinding = true;
    ControlsStatusText = TEXT("Press a new matching input. Escape / View cancels.");
}

void ADiscGolfTourPlayerController::CancelControlBindingCapture()
{
    bWaitingForControlBinding = false;
    ControlsStatusText = TEXT("Binding change cancelled.");
}

bool ADiscGolfTourPlayerController::TryApplyControlBinding(const FKey& NewKey)
{
    FName MappingName;
    const FPlayerKeyMapping* SelectedMapping = GetSelectedPlayerMapping(MappingName);
    UEnhancedInputUserSettings* UserSettings = GetEnhancedInputUserSettings();
    if (!SelectedMapping || !UserSettings)
    {
        ControlsStatusText = TEXT("Input settings are unavailable; no change was saved.");
        bWaitingForControlBinding = false;
        return false;
    }

    if (IsControlsToggleKey(NewKey) || NewKey == EKeys::Tilde)
    {
        ControlsStatusText = TEXT("That key is reserved for menus or the developer console.");
        return false;
    }

    const FKey& ReferenceKey = SelectedMapping->GetDefaultKey().IsValid()
        ? SelectedMapping->GetDefaultKey()
        : SelectedMapping->GetCurrentKey();
    FString CompatibilityReason;
    if (!UDiscGolfInputConfig::IsRemapCandidateCompatible(ReferenceKey, NewKey, CompatibilityReason))
    {
        ControlsStatusText = CompatibilityReason;
        return false;
    }

    TArray<FName> MappingNames;
    TArray<FString> DisplayNames;
    GetControlDefinitions(MappingNames, DisplayNames);
    for (int32 RowIndex = 0; RowIndex < MappingNames.Num(); ++RowIndex)
    {
        TArray<const FPlayerKeyMapping*> ExistingMappings;
        GetSortedMappings(MappingNames[RowIndex], ExistingMappings);
        for (const FPlayerKeyMapping* Existing : ExistingMappings)
        {
            const bool bIsSelected = MappingNames[RowIndex] == MappingName
                && Existing->GetSlot() == SelectedMapping->GetSlot();
            if (!bIsSelected && Existing->GetCurrentKey() == NewKey)
            {
                ControlsStatusText = FString::Printf(TEXT("%s is already assigned to %s."),
                    *NewKey.GetDisplayName(false).ToString(),
                    *DisplayNames[RowIndex]);
                return false;
            }
        }
    }

    FMapPlayerKeyArgs Args;
    Args.MappingName = MappingName;
    Args.Slot = SelectedMapping->GetSlot();
    Args.NewKey = NewKey;
    FGameplayTagContainer FailureReason;
    UserSettings->MapPlayerKey(Args, FailureReason);
    if (!FailureReason.IsEmpty())
    {
        ControlsStatusText = TEXT("Unreal rejected that binding; the previous input was kept.");
        UE_LOG(LogDiscGolfTour, Warning, TEXT("Enhanced Input remap failed for %s: %s"),
            *MappingName.ToString(), *FailureReason.ToStringSimple());
        return false;
    }

    UserSettings->ApplySettings();
    UserSettings->SaveSettings();
    bWaitingForControlBinding = false;
    ControlsStatusText = FString::Printf(TEXT("Saved %s as %s."),
        *NewKey.GetDisplayName(false).ToString(), *MappingName.ToString());
    UE_LOG(LogDiscGolfTour, Log, TEXT("Remapped %s slot %d to %s."),
        *MappingName.ToString(), static_cast<int32>(Args.Slot), *NewKey.ToString());
    return true;
}

void ADiscGolfTourPlayerController::ResetSelectedControl()
{
    FName MappingName;
    if (!GetSelectedPlayerMapping(MappingName)) return;
    UEnhancedInputUserSettings* UserSettings = GetEnhancedInputUserSettings();
    if (!UserSettings) return;

    FMapPlayerKeyArgs Args;
    Args.MappingName = MappingName;
    FGameplayTagContainer FailureReason;
    UserSettings->ResetAllPlayerKeysInRow(Args, FailureReason);
    if (FailureReason.IsEmpty())
    {
        UserSettings->ApplySettings();
        UserSettings->SaveSettings();
        ControlsStatusText = FString::Printf(TEXT("Reset %s to defaults."), *MappingName.ToString());
    }
    else
    {
        ControlsStatusText = TEXT("Could not reset that action.");
    }
}

void ADiscGolfTourPlayerController::ResetAllControls()
{
    UEnhancedInputUserSettings* UserSettings = GetEnhancedInputUserSettings();
    if (!UserSettings) return;

    FGameplayTagContainer FailureReason;
    UserSettings->ResetKeyProfileIdToDefault(UserSettings->GetActiveKeyProfileId(), FailureReason);
    if (FailureReason.IsEmpty())
    {
        UserSettings->ApplySettings();
        UserSettings->SaveSettings();
        ControlsStatusText = TEXT("All controls reset to defaults.");
    }
    else
    {
        ControlsStatusText = TEXT("Could not reset the control profile.");
    }
}
