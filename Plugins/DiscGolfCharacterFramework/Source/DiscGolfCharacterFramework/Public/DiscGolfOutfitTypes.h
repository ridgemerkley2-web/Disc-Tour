#pragma once

#include "CoreMinimal.h"
#include "DiscGolfOutfitTypes.generated.h"

class UMaterialInterface;

UENUM(BlueprintType)
enum class EDGOutfitSlot : uint8
{
    Headwear,
    Eyewear,
    Top,
    Outerwear,
    Bottom,
    Socks,
    Footwear,
    Glove,
    Wrist,
    Bag,
    Accessory
};

UENUM(BlueprintType)
enum class EDGBodyRegion : uint8
{
    Hair,
    Head,
    Neck,
    Torso,
    UpperArms,
    Forearms,
    Hands,
    Hips,
    UpperLegs,
    LowerLegs,
    Feet
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGOutfitVariant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outfit")
    FName VariantId = TEXT("Default");

    // Optional complete material overrides, applied by material index.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outfit")
    TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;

    // Applied to dynamic material instances after overrides.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outfit")
    TMap<FName, FLinearColor> VectorParameters;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outfit")
    TMap<FName, float> ScalarParameters;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGEquippedOutfitEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Outfit")
    EDGOutfitSlot Slot = EDGOutfitSlot::Top;

    // Stable ID from UDiscGolfOutfitItem::ItemId.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Outfit")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Outfit")
    FName VariantId = TEXT("Default");
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGOutfitLoadout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Outfit")
    TArray<FDGEquippedOutfitEntry> Equipped;
};
