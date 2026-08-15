#include "DiscGolfSettingsSubsystem.h"

void UDiscGolfSettingsSubsystem::ApplySettings(
    const FDGUserSettingsProfile& NewSettings)
{
    CurrentSettings = NewSettings;

    CurrentSettings.Input.LookSensitivity = FMath::Max(0.01f, CurrentSettings.Input.LookSensitivity);
    CurrentSettings.Input.StickDeadZone = FMath::Clamp(CurrentSettings.Input.StickDeadZone, 0.0f, 0.95f);
    CurrentSettings.Accessibility.UIScale = FMath::Clamp(CurrentSettings.Accessibility.UIScale, 0.75f, 2.0f);
    CurrentSettings.Accessibility.CameraShakeScale = FMath::Clamp(CurrentSettings.Accessibility.CameraShakeScale, 0.0f, 1.0f);
    CurrentSettings.GameplayAssist.AimAssist01 = FMath::Clamp(CurrentSettings.GameplayAssist.AimAssist01, 0.0f, 1.0f);
    CurrentSettings.GameplayAssist.TimingWindowScale = FMath::Clamp(CurrentSettings.GameplayAssist.TimingWindowScale, 0.5f, 2.0f);
    CurrentSettings.Camera.FieldOfViewDegrees = FMath::Clamp(CurrentSettings.Camera.FieldOfViewDegrees, 60.0f, 120.0f);

    CurrentSettings.Audio.MasterVolume = FMath::Clamp(CurrentSettings.Audio.MasterVolume, 0.0f, 1.0f);
    CurrentSettings.Audio.MusicVolume = FMath::Clamp(CurrentSettings.Audio.MusicVolume, 0.0f, 1.0f);
    CurrentSettings.Audio.EffectsVolume = FMath::Clamp(CurrentSettings.Audio.EffectsVolume, 0.0f, 1.0f);
    CurrentSettings.Audio.AmbienceVolume = FMath::Clamp(CurrentSettings.Audio.AmbienceVolume, 0.0f, 1.0f);
    CurrentSettings.Audio.VoiceVolume = FMath::Clamp(CurrentSettings.Audio.VoiceVolume, 0.0f, 1.0f);

    ApplySettingsToProject(CurrentSettings);
    OnSettingsChanged.Broadcast(CurrentSettings);
}

void UDiscGolfSettingsSubsystem::ResetToDefaults()
{
    ApplySettings(FDGUserSettingsProfile());
}
