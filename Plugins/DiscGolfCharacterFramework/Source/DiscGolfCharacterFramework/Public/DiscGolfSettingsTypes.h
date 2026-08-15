#pragma once

#include "CoreMinimal.h"
#include "DiscGolfSettingsTypes.generated.h"

UENUM(BlueprintType)
enum class EDGColorVisionMode : uint8
{
    None,
    Protanopia,
    Deuteranopia,
    Tritanopia
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGInputUserSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Input")
    float LookSensitivity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Input")
    bool bInvertLookY = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Input")
    bool bSouthpawPreset = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Input")
    bool bHoldToAim = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Input")
    float StickDeadZone = 0.15f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAccessibilitySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Accessibility")
    EDGColorVisionMode ColorVisionMode = EDGColorVisionMode::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Accessibility")
    FLinearColor ShotTracerColor = FLinearColor(0.65f, 0.82f, 0.20f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Accessibility")
    bool bHighContrastUI = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Accessibility")
    bool bSubtitles = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Accessibility")
    float UIScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Accessibility")
    float CameraShakeScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Accessibility")
    bool bReduceMotion = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGGameplayAssistSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Assist", meta=(ClampMin="0.0", ClampMax="1.0"))
    float AimAssist01 = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Assist", meta=(ClampMin="0.5", ClampMax="2.0"))
    float TimingWindowScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Assist")
    bool bShowShotShapeGuide = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Assist")
    bool bShowFlightPreview = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Assist")
    bool bAutoSelectRecommendedDisc = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGCameraUserSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Camera")
    float FieldOfViewDegrees = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Camera")
    bool bAutoFollowDisc = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Camera")
    bool bShowTracer = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Camera")
    float ReplaySpeed = 1.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAudioUserSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Audio")
    float MasterVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Audio")
    float MusicVolume = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Audio")
    float EffectsVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Audio")
    float AmbienceVolume = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Audio")
    float VoiceVolume = 1.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGUserSettingsProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Settings")
    FDGInputUserSettings Input;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Settings")
    FDGAccessibilitySettings Accessibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Settings")
    FDGGameplayAssistSettings GameplayAssist;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Settings")
    FDGCameraUserSettings Camera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Settings")
    FDGAudioUserSettings Audio;
};
