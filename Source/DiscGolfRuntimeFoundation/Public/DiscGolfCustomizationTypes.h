#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfCustomizationTypes.generated.h"

UENUM(BlueprintType)
enum class EDGCosmeticKind : uint8
{
    Hair,
    FacialHair,
    Eyebrow,
    Scar,
    Tattoo,
    Voice,
    PronounSet
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGIdentityProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString DisplayName = TEXT("PLAYER");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EDGHandedness Handedness = EDGHandedness::Right;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName VoiceId = TEXT("voice_default");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName PronounSetId = TEXT("pronouns_default");
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGFaceProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName PresetId = TEXT("face_default");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TMap<FName, float> MorphValues;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGHairProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName HairStyleId = TEXT("hair_none");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName FacialHairId = TEXT("facialhair_none");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName EyebrowId = TEXT("brow_default");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor HairColor = FLinearColor(0.05f, 0.03f, 0.02f, 1.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor FacialHairColor = FLinearColor(0.05f, 0.03f, 0.02f, 1.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor EyebrowColor = FLinearColor(0.05f, 0.03f, 0.02f, 1.0f);
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGAppearanceProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor SkinTone = FLinearColor(0.55f, 0.35f, 0.24f, 1.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor EyeColor = FLinearColor(0.26f, 0.16f, 0.07f, 1.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Complexion = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Freckles = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float SunExposure = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName ScarId = TEXT("scar_none");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FName> TattooIds;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGFullCharacterCustomization
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName AvatarBackendId = TEXT("dg_master");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDGIdentityProfile Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDGBodyProfile Body;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDGBodyBuildProfile BodyBuild;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDGFaceProfile Face;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDGHairProfile Hair;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDGAppearanceProfile Appearance;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDGThrowStyle ThrowStyle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDGOutfitLoadout Outfit;
};
