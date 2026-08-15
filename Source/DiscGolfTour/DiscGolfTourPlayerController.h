#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DiscGolfTourPlayerController.generated.h"

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

protected:
    /** Optional production Enhanced Input assets. Missing/invalid data falls back visibly. */
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UDiscGolfInputConfig> GameplayInputConfig;

private:
    UPROPERTY(Transient)
    TObjectPtr<UDiscGolfInputConfig> EffectiveInputConfig;

    bool bGameplayContextAdded = false;
    bool bReportedInputFallback = false;
    bool bControlsMenuOpen = false;
    bool bWaitingForControlBinding = false;
    bool bSettingsPage = true;
    bool bWasPausedBeforeControlsMenu = false;
    int32 SelectedControlIndex = 0;
    int32 SelectedBindingIndex = 0;
    FString ControlsStatusText;

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
    FDiscGolfPlayerSettings GetPlayerSettings() const;
    void CommitPlayerSettings(const FDiscGolfPlayerSettings& Settings);
    void GetControlDefinitions(TArray<FName>& OutMappingNames, TArray<FString>& OutDisplayNames) const;
    void GetSortedMappings(FName MappingName, TArray<const FPlayerKeyMapping*>& OutMappings) const;
    const FPlayerKeyMapping* GetSelectedPlayerMapping(FName& OutMappingName) const;
};
