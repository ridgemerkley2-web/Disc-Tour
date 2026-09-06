#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DiscGolfTypes.h"
#include "DiscTrajectoryTypes.h"
#include "DiscGolfPlayerExperience.generated.h"

UENUM(BlueprintType)
enum class EDiscGolfUnitSystem : uint8
{
    Imperial,
    Metric
};

UENUM(BlueprintType)
enum class EDiscGolfColorVisionMode : uint8
{
    None,
    Protanopia,
    Deuteranopia,
    Tritanopia
};

UENUM(BlueprintType)
enum class EDiscGolfTracerColorPreset : uint8
{
    SignalTeal,
    AccessibleLime,
    TournamentAmber,
    PaperWhite
};

UENUM(BlueprintType)
enum class EDiscGolfLandingClassification : uint8
{
    Fairway,
    SemiRough,
    DeepRough,
    Green,
    OutOfBounds,
    Hazard
};

/** Persistent, asset-independent settings hooks for the playable vertical slice. */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfPlayerSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 SchemaVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 GraphicsQuality = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 ResolutionX = 1920;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 ResolutionY = 1080;
    /** 0 fullscreen, 1 windowed fullscreen, 2 windowed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 WindowMode = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float MasterVolume = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float MusicVolume = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float EffectsVolume = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float AmbienceVolume = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float VoiceVolume = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float MouseSensitivity = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float ControllerSensitivity = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float ControllerDeadZone = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bInvertY = false;
    /** Controller layout only; character throwing handedness remains independent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bSouthpawController = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float CameraShakeStrength = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bAutoFollowDisc = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float ReplaySpeed = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bHudVisible = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bBasketMarkerVisible = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EDiscGolfUnitSystem Units = EDiscGolfUnitSystem::Imperial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float HudScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float TextScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bReducedMotion = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bHighContrastBasketMarker = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bHighContrastUI = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bSubtitles = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EDiscGolfColorVisionMode ColorVisionMode = EDiscGolfColorVisionMode::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EDiscGolfTracerColorPreset TracerColorPreset = EDiscGolfTracerColorPreset::SignalTeal;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bAimingIndicatorVisible = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float AimAssist01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float TimingWindowScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bShotShapeGuide = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bOptionalFlightPreview = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bHoldForTiming = false;

    void Normalize();
};

/** One compact, queryable record per authoritative throw; no frame stream is duplicated here. */
USTRUCT(BlueprintType)
struct FDiscGolfThrowHistoryEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 HoleNumber = 1;
    UPROPERTY(BlueprintReadOnly) int32 ShotNumber = 1;
    UPROPERTY(BlueprintReadOnly) FName DiscId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FText DiscName;
    UPROPERTY(BlueprintReadOnly) EDiscPlastic Plastic = EDiscPlastic::Tour;
    UPROPERTY(BlueprintReadOnly) float DiscWeightGrams = 175.0f;
    UPROPERTY(BlueprintReadOnly) float ReleaseSpeedMps = 0.0f;
    UPROPERTY(BlueprintReadOnly) float HyzerAngleDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) float NoseAngleDeg = 0.0f;
    UPROPERTY(BlueprintReadOnly) FVector LaunchDirection = FVector::ForwardVector;
    UPROPERTY(BlueprintReadOnly) FVector WindMps = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float CarryDistanceMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float TotalDistanceMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RemainingDistanceMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) EDiscGolfLandingClassification Landing = EDiscGolfLandingClassification::Fairway;
    UPROPERTY(BlueprintReadOnly) EDiscGolfPenaltyType Penalty = EDiscGolfPenaltyType::None;
    UPROPERTY(BlueprintReadOnly) int32 PenaltyStrokes = 0;
    UPROPERTY(BlueprintReadOnly) FVector LieLocationCm = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bHoledOut = false;
};

USTRUCT(BlueprintType)
struct FDiscGolfShotPresentationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bValid = false;
    UPROPERTY(BlueprintReadOnly) EDiscGolfLandingClassification Landing = EDiscGolfLandingClassification::Fairway;
    UPROPERTY(BlueprintReadOnly) float CarryMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float TotalMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) float RemainingMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly) EDiscGolfPenaltyType Penalty = EDiscGolfPenaltyType::None;
    UPROPERTY(BlueprintReadOnly) bool bHoledOut = false;
};

UCLASS()
class DISCGOLFTOUR_API UDiscGolfThrowHistorySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxHistoryEntries = 128;

    UFUNCTION(BlueprintCallable) void RecordThrow(const FDiscGolfThrowHistoryEntry& Entry);
    UFUNCTION(BlueprintCallable) void ResetHistory() { Entries.Reset(); }
    UFUNCTION(BlueprintPure) TArray<FDiscGolfThrowHistoryEntry> GetThrowHistory() const { return Entries; }
    UFUNCTION(BlueprintPure) int32 GetThrowCount() const { return Entries.Num(); }

private:
    UPROPERTY() TArray<FDiscGolfThrowHistoryEntry> Entries;
};

namespace DiscGolfPlayerExperience
{
    DISCGOLFTOUR_API FString FormatDistance(float DistanceMeters, EDiscGolfUnitSystem Units, bool bIncludeToPin = false);
    DISCGOLFTOUR_API FString FormatWind(float SpeedMps, EDiscGolfUnitSystem Units);
    DISCGOLFTOUR_API FString HoleScoreName(int32 Strokes, int32 Par);
    DISCGOLFTOUR_API FString DiscClassName(int32 Speed);
    DISCGOLFTOUR_API FString LandingName(EDiscGolfLandingClassification Landing);
    DISCGOLFTOUR_API EDiscGolfLandingClassification ClassifyLanding(const FDiscGolfLieState& Lie, bool bHoledOut);
    DISCGOLFTOUR_API float BasketMarkerOpacity(
        const FDiscGolfPlayerSettings& Settings,
        bool bBasketDirectlyVisible,
        float DistanceMeters);
    DISCGOLFTOUR_API FLinearColor ResolveTracerColor(EDiscGolfTracerColorPreset Preset);
    DISCGOLFTOUR_API FString ColorVisionModeName(EDiscGolfColorVisionMode Mode);
    DISCGOLFTOUR_API FString TracerColorPresetName(EDiscGolfTracerColorPreset Preset);
    DISCGOLFTOUR_API void BuildBoundedReplaySamples(
        const TArray<FDiscTrajectorySample>& Source,
        TArray<FDiscTrajectorySample>& OutSamples,
        int32 MaxSamples = 1800);
}
