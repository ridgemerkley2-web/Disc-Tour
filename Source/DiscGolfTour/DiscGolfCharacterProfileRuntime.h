#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfCharacterProfileRuntime.generated.h"

/**
 * Project-owned creator limits. These intentionally match
 * Plugins/DiscGolfCharacterFramework/Config/DG_CharacterCreatorSchema.json,
 * which is narrower than some of the framework struct's editor-only clamps.
 */
namespace DiscGolfCharacterCreatorSchema
{
    inline constexpr float MinHeightCm = 150.0f;
    inline constexpr float MaxHeightCm = 210.0f;
    inline constexpr float DefaultHeightCm = 183.0f;

    inline constexpr float MinWingspanScale = 0.92f;
    inline constexpr float MaxWingspanScale = 1.08f;
    inline constexpr float MinShoulderWidthScale = 0.92f;
    inline constexpr float MaxShoulderWidthScale = 1.08f;
    inline constexpr float MinTorsoLengthScale = 0.94f;
    inline constexpr float MaxTorsoLengthScale = 1.06f;
    inline constexpr float MinLegLengthScale = 0.94f;
    inline constexpr float MaxLegLengthScale = 1.06f;
    inline constexpr float MinHandScale = 0.94f;
    inline constexpr float MaxHandScale = 1.06f;
    inline constexpr float DefaultBodyScale = 1.0f;

    inline constexpr float MinMassKg = 45.0f;
    inline constexpr float MaxMassKg = 160.0f;
    inline constexpr float DefaultMassKg = 82.0f;

    inline constexpr float MinStyleValue = 0.0f;
    inline constexpr float MaxStyleValue = 1.0f;
    inline constexpr float DefaultRunUpIntensity = 0.65f;
    inline constexpr float DefaultReachBackAmount = 0.80f;
    inline constexpr float DefaultTorsoRotation = 0.75f;
    inline constexpr float DefaultBraceIntensity = 0.75f;
    inline constexpr float DefaultExplosiveness = 0.60f;
    inline constexpr float DefaultFollowThrough = 0.80f;

    inline constexpr float DefaultMuscularity = 0.35f;
    inline constexpr float DefaultBodyFat = 0.35f;
    inline constexpr float DefaultBodyShape = 0.0f;
}

/**
 * Stable, asset-independent persistence payload for the Session 4 creator.
 *
 * Save data deliberately contains only primitives. The installed character
 * framework remains the runtime type authority, and conversion is explicit at
 * the project boundary. In particular, gameplay-affecting throw inputs and the
 * framework PowerMultiplier/SpinMultiplier are not serialized here.
 */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfCharacterProfileSaveData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Identity")
    bool bLeftHanded = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body")
    float HeightCm = DiscGolfCharacterCreatorSchema::DefaultHeightCm;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body")
    float WingspanScale = DiscGolfCharacterCreatorSchema::DefaultBodyScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body")
    float ShoulderWidthScale = DiscGolfCharacterCreatorSchema::DefaultBodyScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body")
    float TorsoLengthScale = DiscGolfCharacterCreatorSchema::DefaultBodyScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body")
    float LegLengthScale = DiscGolfCharacterCreatorSchema::DefaultBodyScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body")
    float HandScale = DiscGolfCharacterCreatorSchema::DefaultBodyScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body")
    float MassKg = DiscGolfCharacterCreatorSchema::DefaultMassKg;

    // Persisted now as the stable data foundation; production proxy morphing is deferred.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body Build")
    float Muscularity = DiscGolfCharacterCreatorSchema::DefaultMuscularity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body Build")
    float BodyFat = DiscGolfCharacterCreatorSchema::DefaultBodyFat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body Build")
    float Chest = DiscGolfCharacterCreatorSchema::DefaultBodyShape;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body Build")
    float Waist = DiscGolfCharacterCreatorSchema::DefaultBodyShape;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body Build")
    float Hips = DiscGolfCharacterCreatorSchema::DefaultBodyShape;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body Build")
    float Arms = DiscGolfCharacterCreatorSchema::DefaultBodyShape;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Body Build")
    float Legs = DiscGolfCharacterCreatorSchema::DefaultBodyShape;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Throw Style")
    float RunUpIntensity = DiscGolfCharacterCreatorSchema::DefaultRunUpIntensity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Throw Style")
    float ReachBackAmount = DiscGolfCharacterCreatorSchema::DefaultReachBackAmount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Throw Style")
    float TorsoRotation = DiscGolfCharacterCreatorSchema::DefaultTorsoRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Throw Style")
    float BraceIntensity = DiscGolfCharacterCreatorSchema::DefaultBraceIntensity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Throw Style")
    float Explosiveness = DiscGolfCharacterCreatorSchema::DefaultExplosiveness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character Creator|Throw Style")
    float FollowThrough = DiscGolfCharacterCreatorSchema::DefaultFollowThrough;

    /** Enforces finite values and the installed creator schema's supported ranges. */
    void Sanitize();

    FDGBodyProfile ToBodyProfile() const;
    FDGBodyBuildProfile ToBodyBuildProfile() const;
    FDGThrowStyle ToThrowStyle() const;
    EDGHandedness GetHandedness() const;

    static FDiscGolfCharacterProfileSaveData FromFramework(
        const FDGBodyProfile& Body,
        const FDGThrowStyle& ThrowStyle,
        EDGHandedness Handedness,
        const FDGBodyBuildProfile& BodyBuild = FDGBodyBuildProfile());
};
