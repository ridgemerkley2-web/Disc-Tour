#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfFullCharacterRuntime.h"
#include "DiscGolfInputRoutePolicy.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfRoundFlowPresentation.h"
#include "GameFramework/PlayerController.h"
#include "DiscGolfTourPlayerController.generated.h"

class UDiscGolfCharacterCreatorWidget;
class UDiscGolfInputConfig;
class UDiscGolfRoundFlowWidget;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputUserSettings;
class UInputAction;
struct FInputKeyEventArgs;
struct FPlayerKeyMapping;
struct FDiscGolfPlayerSettings;

struct FDiscGolfControlBindingRow
{
    FString DisplayName;
    TArray<FString> BindingLabels;
};

struct FDiscGolfSettingRow
{
    FString Label;
    FString Value;
};

UCLASS()
class DISCGOLFTOUR_API ADiscGolfTourPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ADiscGolfTourPlayerController();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;

    /** Returns the assigned content config, or a safe source-built fallback. */
    const UDiscGolfInputConfig* EnsureGameplayInputReady();

    bool IsControlsMenuOpen() const { return bControlsMenuOpen; }
    bool IsWaitingForControlBinding() const { return bWaitingForControlBinding; }
    bool IsSettingsPageOpen() const { return bSettingsPage; }
    int32 GetSelectedControlIndex() const { return SelectedControlIndex; }
    int32 GetSelectedSettingIndex() const { return SelectedSettingIndex; }
    int32 GetSelectedBindingIndex() const { return SelectedBindingIndex; }
    const FString& GetControlsStatusText() const { return ControlsStatusText; }
    void GetControlBindingRows(TArray<FDiscGolfControlBindingRow>& OutRows) const;
    void GetSettingRows(TArray<FDiscGolfSettingRow>& OutRows) const;
    FDiscGolfPlayerSettings GetCurrentPlayerSettings() const;
    EDiscGolfInputRoute GetActiveInputRoute() const;
    bool IsInputRouteAllowed(EDiscGolfInputRoute ActionRoute) const;
    /** True until every Space/South intro-dismiss edge has been released. */
    bool IsPresentationDismissReleasePending() const
    {
        return PresentationDismissInputBarrier.IsPending();
    }
    /** True until an explicit new protected-key down edge proves fresh intent. */
    bool IsFreshThrowDownRequiredAfterPresentation() const
    {
        return bFreshThrowDownRequiredAfterPresentation;
    }

    /** Applies the native front-end or gameplay input mode without a UI asset. */
    void ApplyMainMenuInputMode(bool bMenuVisible);

    /** Reconciles the native round-flow widget with authoritative GameMode state. */
    void RefreshRoundFlowPresentation();

    /** Executes one validated presentation request through existing GameMode actions. */
    bool HandleRoundFlowAction(EDGRoundFlowAction Action);

    /** True only when an attached native widget has a usable focus target. */
    bool HasInteractiveRoundFlowWidget() const;

    /** Development probe for settings -> originating round-flow restoration. */
    bool RunRoundFlowSettingsRecoveryProbe(FString& OutError);

    /** Development-only live probe used by the Session 16 temporary-state gate. */
    bool RunPlayabilityPauseResumeProbe(FString& OutError);

    /** Opens the event-driven character/outfit editor on the existing possessed pawn. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    bool OpenCharacterCreator();

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character Creator")
    bool IsCharacterCreatorOpen() const { return bCharacterCreatorOpen; }

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    bool PreviewCharacterCreatorDraft(
        const FDGBodyProfile& Body,
        const FDGThrowStyle& ThrowStyle,
        EDGHandedness Handedness);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    bool PreviewFullCharacterCreatorDraft(
        const FDGFullCharacterCustomization& CharacterCustomization);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    bool LoadCharacterCreatorPreset(
        FName PresetId,
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutThrowStyle,
        EDGHandedness& OutHandedness);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    bool ResetCharacterCreatorDraft(
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutThrowStyle,
        EDGHandedness& OutHandedness);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    bool ApplyCharacterCreatorDraft(
        const FDGBodyProfile& Body,
        const FDGThrowStyle& ThrowStyle,
        EDGHandedness Handedness);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    bool ApplyFullCharacterCreatorDraft(
        const FDGFullCharacterCustomization& CharacterCustomization);

    bool ResetCharacterCreatorCurrentTab(
        int32 TabIndex,
        FDGFullCharacterCustomization& OutDraft);
    bool ResetCharacterCreatorAll(FDGFullCharacterCustomization& OutDraft);
    bool RandomizeCharacterCreatorDraft(
        const FDiscGolfCharacterRandomizeLocks& Locks,
        FDGFullCharacterCustomization& OutDraft);
    bool ApplyCharacterCreatorFacePreset(
        FName PresetId,
        FDGFullCharacterCustomization& OutDraft);
    bool SelectCharacterCreatorCosmetic(
        EDGCosmeticKind Kind,
        FName ItemId,
        FDGFullCharacterCustomization& OutDraft);
    TArray<FDiscGolfCosmeticOption> GetCharacterCreatorCosmeticOptions(
        EDGCosmeticKind Kind) const;
    bool SelectCharacterCreatorBackend(
        FName BackendId,
        FDGFullCharacterCustomization& OutDraft);
    bool IsCharacterCreatorMetaHumanBackendAvailable() const;
    FString GetCharacterCreatorBackendStatusText() const;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    void CancelCharacterCreator();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    void RotateCharacterCreatorPreview(float DeltaYawDegrees);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    void ZoomCharacterCreatorPreview(float DeltaArmLength);

    bool PreviewCharacterCreatorOutfitSelection(
        EDGOutfitSlot Slot,
        FName ItemId,
        FName VariantId,
        FDGOutfitLoadout& OutDraft);
    bool ResetCharacterCreatorOutfit(FDGOutfitLoadout& OutDraft);
    bool RandomizeCharacterCreatorOutfit(FDGOutfitLoadout& OutDraft);
    TArray<FDiscGolfOutfitOption> GetCharacterCreatorOutfitOptions(EDGOutfitSlot Slot) const;
    bool PrepareCharacterCreatorForSession6VisualEvidence(EDGOutfitSlot Slot);
    const FDGOutfitLoadout& GetCharacterCreatorDraftOutfit() const
    {
        return CharacterCreatorDraftOutfit;
    }
    const FDGFullCharacterCustomization& GetCharacterCreatorDraftCustomization() const
    {
        return CharacterCreatorDraftCustomization;
    }
    bool PrepareCharacterCreatorForSession7VisualEvidence(
        int32 TabIndex,
        const FDGFullCharacterCustomization& Draft);
    int32 GetCharacterCreatorActiveTabIndex() const;
    void GetSession7CharacterCreatorVisibleControlIds(
        TArray<FString>& OutControlIds) const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character Creator")
    FString GetCharacterCreatorStatusText() const { return CharacterCreatorStatusText; }

protected:
    /** Optional production Enhanced Input assets. Missing/invalid data falls back visibly. */
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UDiscGolfInputConfig> GameplayInputConfig;

    /** Optional authored shell. If unset, the fully functional native widget is used. */
    UPROPERTY(EditDefaultsOnly, Category="Character Creator")
    TSubclassOf<UDiscGolfCharacterCreatorWidget> CharacterCreatorWidgetClass;

private:
    UPROPERTY(Transient)
    TObjectPtr<UDiscGolfInputConfig> EffectiveInputConfig;

    UPROPERTY(Transient)
    TObjectPtr<UDiscGolfCharacterCreatorWidget> CharacterCreatorWidget;

    UPROPERTY(Transient)
    TObjectPtr<UDiscGolfRoundFlowWidget> RoundFlowWidget;

    bool bGameplayContextAdded = false;
    bool bReportedInputFallback = false;
    bool bUsingRuntimeInputFallback = false;
    bool bAppliedSouthpawController = false;
    float AppliedControllerDeadZone = 0.25f;
    bool bControlsMenuOpen = false;
    bool bWaitingForControlBinding = false;
    bool bSettingsPage = true;
    bool bWasPausedBeforeControlsMenu = false;
    bool bControlsMenuSuspendedPlayabilityMonitor = false;
    bool bCharacterCreatorOpen = false;
    bool bCharacterCreatorPreviousMouseCursor = false;
    bool bHasRoundFlowSnapshot = false;
    bool bRefreshingRoundFlowPresentation = false;
    bool bHandlingRoundFlowAction = false;
    bool bRoundFlowSettingsOriginValid = false;
    bool bRoundFlowWidgetCreationAttempted = false;
    bool bRecoverPresentationDismissAfterReactivation = false;
    bool bPresentationDismissAwaitingReactivationNeutral = false;
    bool bFreshThrowDownRequiredAfterPresentation = false;
    uint64 PresentationDismissReactivationFrame = 0;
    FDiscGolfPresentationDismissInputBarrier PresentationDismissInputBarrier;
    struct FPendingPresentationDismissReleaseCompletion
    {
        uint32 Generation = 0;
        uint64 ReleaseFrame = 0;
    };
    TMap<FKey, FPendingPresentationDismissReleaseCompletion>
        PendingPresentationDismissReleaseCompletions;
    EDGRoundFlowScreen RoundFlowSettingsOrigin = EDGRoundFlowScreen::Hidden;
    EDGRoundFlowScreen RoundFlowWidgetAttemptedScreen = EDGRoundFlowScreen::Hidden;
    FDGRoundFlowSnapshot ActiveRoundFlowSnapshot;
    FString RoundFlowLastError;
    int32 SelectedControlIndex = 0;
    int32 SelectedSettingIndex = 0;
    int32 SelectedBindingIndex = 0;
    FString ControlsStatusText;
    FString CharacterCreatorStatusText;
    FDGBodyProfile CharacterCreatorOpeningBody;
    FDGThrowStyle CharacterCreatorOpeningThrowStyle;
    EDGHandedness CharacterCreatorOpeningHandedness = EDGHandedness::Right;
    FDGOutfitLoadout CharacterCreatorOpeningOutfit;
    FDGOutfitLoadout CharacterCreatorDraftOutfit;
    FDGFullCharacterCustomization CharacterCreatorOpeningCustomization;
    FDGFullCharacterCustomization CharacterCreatorDraftCustomization;
    mutable bool bMetaHumanBackendAvailabilityChecked = false;
    mutable bool bMetaHumanBackendAvailable = false;
    mutable FString MetaHumanBackendAvailabilityStatus;

    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;
    UEnhancedInputUserSettings* GetEnhancedInputUserSettings() const;
    void SetGameplayContextEnabled(bool bEnabled);
    void CompletePresentationDismissRelease(FKey ReleasedKey, uint32 ReleaseGeneration);
    void RestoreGameplayContextAfterPresentationDismiss();
    void HandleApplicationWillDeactivate();
    void HandleApplicationHasReactivated();
    void GetPresentationTransitionProtectedKeys(TArray<FKey>& OutKeys) const;
    bool IsPresentationTransitionProtectedKey(const FKey& Key) const;
    bool IsCurrentThrowInputKey(const FKey& Key) const;
    void GetPressedPresentationTransitionKeys(TArray<FKey>& OutKeys) const;
    void ArmPresentationTransitionKey(const FKey& Key);
    bool BuildRoundFlowState(FDGRoundFlowState& OutState, FString& OutError) const;
    bool OpenSettingsFromRoundFlow();
    void ApplyRoundFlowInputMode();
    void RemoveRoundFlowWidget();
    void RefreshRuntimeInputFromPlayerSettings();
    void OpenControlsMenu();
    void CloseControlsMenu();
    void MoveControlSelection(int32 Direction);
    void MoveBindingSelection(int32 Direction);
    void BeginControlBindingCapture();
    void CancelControlBindingCapture();
    bool TryApplyControlBinding(const FKey& NewKey);
    void ResetSelectedControl();
    void ResetAllControls();
    void ToggleMenuPage();
    void MoveSettingsSelection(int32 Direction);
    void AdjustSelectedSetting(int32 Direction);
    void CommitCharacterCreatorInputMode();
    bool CloseCharacterCreator(bool bRestoreOpeningProfile);
    void SyncLegacyCreatorDraftsFromFull();
    void CacheCharacterCreatorMetaHumanAvailability() const;
    bool ResolveCharacterCreatorPreset(
        FName PresetId,
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutThrowStyle,
        EDGHandedness& OutHandedness) const;
    FDiscGolfPlayerSettings GetPlayerSettings() const;
    void CommitPlayerSettings(const FDiscGolfPlayerSettings& Settings);
    void GetControlDefinitions(TArray<FName>& OutMappingNames, TArray<FString>& OutDisplayNames) const;
    void GetSortedMappings(FName MappingName, TArray<const FPlayerKeyMapping*>& OutMappings) const;
    const FPlayerKeyMapping* GetSelectedPlayerMapping(FName& OutMappingName) const;
};
