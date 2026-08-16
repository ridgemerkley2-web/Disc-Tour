#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitRuntime.h"
#include "GameFramework/PlayerController.h"
#include "DiscGolfTourPlayerController.generated.h"

class UDiscGolfCharacterCreatorWidget;
class UDiscGolfInputConfig;
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

    /** Returns the assigned content config, or a safe source-built fallback. */
    const UDiscGolfInputConfig* EnsureGameplayInputReady();

    bool IsControlsMenuOpen() const { return bControlsMenuOpen; }
    bool IsWaitingForControlBinding() const { return bWaitingForControlBinding; }
    bool IsSettingsPageOpen() const { return bSettingsPage; }
    int32 GetSelectedControlIndex() const { return SelectedControlIndex; }
    int32 GetSelectedBindingIndex() const { return SelectedBindingIndex; }
    const FString& GetControlsStatusText() const { return ControlsStatusText; }
    void GetControlBindingRows(TArray<FDiscGolfControlBindingRow>& OutRows) const;
    void GetSettingRows(TArray<FDiscGolfSettingRow>& OutRows) const;

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
    void CancelCharacterCreator();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character Creator")
    void RotateCharacterCreatorPreview(float DeltaYawDegrees);

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

    bool bGameplayContextAdded = false;
    bool bReportedInputFallback = false;
    bool bControlsMenuOpen = false;
    bool bWaitingForControlBinding = false;
    bool bSettingsPage = true;
    bool bWasPausedBeforeControlsMenu = false;
    bool bCharacterCreatorOpen = false;
    bool bCharacterCreatorPreviousMouseCursor = false;
    int32 SelectedControlIndex = 0;
    int32 SelectedBindingIndex = 0;
    FString ControlsStatusText;
    FString CharacterCreatorStatusText;
    FDGBodyProfile CharacterCreatorOpeningBody;
    FDGThrowStyle CharacterCreatorOpeningThrowStyle;
    EDGHandedness CharacterCreatorOpeningHandedness = EDGHandedness::Right;
    FDGOutfitLoadout CharacterCreatorOpeningOutfit;
    FDGOutfitLoadout CharacterCreatorDraftOutfit;

    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;
    UEnhancedInputUserSettings* GetEnhancedInputUserSettings() const;
    void SetGameplayContextEnabled(bool bEnabled);
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
    void CloseCharacterCreator(bool bRestoreOpeningProfile);
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
