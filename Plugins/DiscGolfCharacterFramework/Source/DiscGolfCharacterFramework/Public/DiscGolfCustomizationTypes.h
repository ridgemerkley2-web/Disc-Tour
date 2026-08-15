#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfCustomizationTypes.generated.h"

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGIdentityProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Identity")
    FString DisplayName = TEXT("Player");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Identity")
    EDGHandedness Handedness = EDGHandedness::Right;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Identity")
    FName VoiceId = TEXT("voice_default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Identity")
    FName PronounSetId = TEXT("pronouns_default");
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGBodyBuildProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Body", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Muscularity = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Body", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BodyFat = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Body", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Chest = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Body", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Waist = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Body", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Hips = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Body", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Arms = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Body", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Legs = 0.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGFaceProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Face")
    FName PresetId = TEXT("face_default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Face")
    TMap<FName, float> MorphValues;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGHairProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Hair")
    FName HairStyleId = TEXT("hair_none");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Hair")
    FName FacialHairId = TEXT("facialhair_none");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Hair")
    FName EyebrowId = TEXT("brow_default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Hair")
    FLinearColor HairColor = FLinearColor(0.05f, 0.03f, 0.02f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Hair")
    FLinearColor FacialHairColor = FLinearColor(0.05f, 0.03f, 0.02f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Hair")
    FLinearColor EyebrowColor = FLinearColor(0.05f, 0.03f, 0.02f, 1.0f);
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAppearanceProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
    FLinearColor SkinTone = FLinearColor(0.55f, 0.35f, 0.24f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
    FLinearColor EyeColor = FLinearColor(0.15f, 0.24f, 0.20f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Complexion = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Freckles = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SunExposure = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
    FName ScarId = TEXT("scar_none");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Appearance")
    TArray<FName> TattooIds;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGFullCharacterCustomization
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGIdentityProfile Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGBodyProfile Body;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGBodyBuildProfile BodyBuild;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGFaceProfile Face;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGHairProfile Hair;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGAppearanceProfile Appearance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGThrowStyle ThrowStyle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGOutfitLoadout Outfit;
};
