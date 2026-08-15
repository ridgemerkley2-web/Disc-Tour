#include "DiscGolfTourPlayerController.h"

#include "DiscGolfInputConfig.h"
#include "DiscGolfPlayerExperience.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTour.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameplayTagContainer.h"
#include "InputKeyEventArgs.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
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
}

void ADiscGolfTourPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = false;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
    EnsureGameplayInputReady();
}

void ADiscGolfTourPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

    if (!bControlsMenuOpen && bPressed && Params.Key == EKeys::F9)
    {
        if (ADiscGolfTourGameMode* GameMode = GetWorld()
            ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr)
        {
            GameMode->ToggleDeveloperHud();
        }
        return true;
    }

    if (!bControlsMenuOpen && bPressed
        && IsAnyOf(Params.Key, {EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom}))
    {
        if (ADiscGolfTourGameMode* GameMode = GetWorld()
            ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
            GameMode && GameMode->IsHoleIntroVisible())
        {
            GameMode->SkipCurrentPresentation();
            return true;
        }
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
            else if (IsAnyOf(Params.Key, {EKeys::Tab, EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder}))
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
                if (!bSettingsPage) BeginControlBindingCapture();
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

    if (bPressed && IsControlsToggleKey(Params.Key))
    {
        OpenControlsMenu();
        return true;
    }

    return Super::InputKey(Params);
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

            EffectiveInputConfig = UDiscGolfInputConfig::BuildRuntimeFallback(this);
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

    if (!bGameplayContextAdded && IsLocalController())
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

void ADiscGolfTourPlayerController::SetGameplayContextEnabled(bool bEnabled)
{
    if (!EffectiveInputConfig || !EffectiveInputConfig->GameplayMappingContext) return;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetEnhancedInputSubsystem())
    {
        if (bEnabled && !bGameplayContextAdded)
        {
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

void ADiscGolfTourPlayerController::OpenControlsMenu()
{
    EnsureGameplayInputReady();
    bControlsMenuOpen = true;
    bWaitingForControlBinding = false;
    bWasPausedBeforeControlsMenu = GetWorld() && GetWorld()->IsPaused();
    ControlsStatusText = TEXT("Player settings. Tab / Shoulders switches to controls.");
    SetGameplayContextEnabled(false);
    if (!bWasPausedBeforeControlsMenu)
    {
        SetPause(true);
    }
    FlushPressedKeys();
}

FDiscGolfPlayerSettings ADiscGolfTourPlayerController::GetPlayerSettings() const
{
    const UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    return Instance ? Instance->GetPlayerSettings() : FDiscGolfPlayerSettings();
}

void ADiscGolfTourPlayerController::CommitPlayerSettings(const FDiscGolfPlayerSettings& Settings)
{
    if (UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance()))
    {
        Instance->UpdatePlayerSettings(Settings);
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
    constexpr int32 SettingCount = 18;
    SelectedControlIndex = (SelectedControlIndex + Direction + SettingCount) % SettingCount;
}

void ADiscGolfTourPlayerController::AdjustSelectedSetting(int32 Direction)
{
    FDiscGolfPlayerSettings Settings = GetPlayerSettings();
    switch (SelectedControlIndex)
    {
        case 0: Settings.GraphicsQuality += Direction; break;
        case 1: Settings.ResolutionX = Direction > 0 ? 2560 : 1920; Settings.ResolutionY = Direction > 0 ? 1440 : 1080; break;
        case 2: Settings.WindowMode += Direction; break;
        case 3: Settings.MasterVolume += Direction * 0.05f; break;
        case 4: Settings.MusicVolume += Direction * 0.05f; break;
        case 5: Settings.EffectsVolume += Direction * 0.05f; break;
        case 6: Settings.MouseSensitivity += Direction * 0.10f; break;
        case 7: Settings.ControllerSensitivity += Direction * 0.10f; break;
        case 8: Settings.bInvertY = !Settings.bInvertY; break;
        case 9: Settings.CameraShakeStrength += Direction * 0.10f; break;
        case 10: Settings.bHudVisible = !Settings.bHudVisible; break;
        case 11: Settings.bBasketMarkerVisible = !Settings.bBasketMarkerVisible; break;
        case 12: Settings.Units = Settings.Units == EDiscGolfUnitSystem::Imperial ? EDiscGolfUnitSystem::Metric : EDiscGolfUnitSystem::Imperial; break;
        case 13: Settings.HudScale += Direction * 0.05f; break;
        case 14: Settings.TextScale += Direction * 0.05f; break;
        case 15: Settings.bReducedMotion = !Settings.bReducedMotion; break;
        case 16: Settings.bHighContrastBasketMarker = !Settings.bHighContrastBasketMarker; break;
        case 17: Settings.bAimingIndicatorVisible = !Settings.bAimingIndicatorVisible; break;
        default: break;
    }
    Settings.Normalize();
    CommitPlayerSettings(Settings);
    ControlsStatusText = TEXT("Setting saved to the local profile.");
}

void ADiscGolfTourPlayerController::GetSettingRows(TArray<FDiscGolfSettingRow>& OutRows) const
{
    const FDiscGolfPlayerSettings S = GetPlayerSettings();
    const TCHAR* Quality[] = {TEXT("PERFORMANCE"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("CINEMATIC")};
    const TCHAR* Window[] = {TEXT("FULLSCREEN"), TEXT("BORDERLESS"), TEXT("WINDOWED")};
    const auto Add = [&OutRows](const TCHAR* Label, const FString& Value)
    {
        FDiscGolfSettingRow& Row = OutRows.AddDefaulted_GetRef(); Row.Label = Label; Row.Value = Value;
    };
    OutRows.Reset(18);
    Add(TEXT("GRAPHICS QUALITY"), Quality[FMath::Clamp(S.GraphicsQuality, 0, 3)]);
    Add(TEXT("RESOLUTION"), FString::Printf(TEXT("%d X %d"), S.ResolutionX, S.ResolutionY));
    Add(TEXT("WINDOW MODE"), Window[FMath::Clamp(S.WindowMode, 0, 2)]);
    Add(TEXT("MASTER VOLUME"), FString::Printf(TEXT("%.0f%%"), S.MasterVolume * 100.0f));
    Add(TEXT("MUSIC VOLUME"), FString::Printf(TEXT("%.0f%%"), S.MusicVolume * 100.0f));
    Add(TEXT("EFFECTS VOLUME"), FString::Printf(TEXT("%.0f%%"), S.EffectsVolume * 100.0f));
    Add(TEXT("MOUSE SENSITIVITY"), FString::Printf(TEXT("%.1f"), S.MouseSensitivity));
    Add(TEXT("CONTROLLER SENSITIVITY"), FString::Printf(TEXT("%.1f"), S.ControllerSensitivity));
    Add(TEXT("INVERT Y"), S.bInvertY ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("CAMERA SHAKE"), FString::Printf(TEXT("%.0f%%"), S.CameraShakeStrength * 100.0f));
    Add(TEXT("HUD"), S.bHudVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
    Add(TEXT("BASKET MARKER"), S.bBasketMarkerVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
    Add(TEXT("UNITS"), S.Units == EDiscGolfUnitSystem::Imperial ? TEXT("FEET / IMPERIAL") : TEXT("METERS / METRIC"));
    Add(TEXT("HUD SCALE"), FString::Printf(TEXT("%.0f%%"), S.HudScale * 100.0f));
    Add(TEXT("TEXT SCALE"), FString::Printf(TEXT("%.0f%%"), S.TextScale * 100.0f));
    Add(TEXT("REDUCED MOTION"), S.bReducedMotion ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("HIGH-CONTRAST MARKER"), S.bHighContrastBasketMarker ? TEXT("ON") : TEXT("OFF"));
    Add(TEXT("AIM INDICATOR"), S.bAimingIndicatorVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
}

void ADiscGolfTourPlayerController::CloseControlsMenu()
{
    bControlsMenuOpen = false;
    bWaitingForControlBinding = false;
    SetGameplayContextEnabled(true);
    if (!bWasPausedBeforeControlsMenu)
    {
        SetPause(false);
    }
    FlushPressedKeys();
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
