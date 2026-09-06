#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.generated.h"

UENUM(BlueprintType)
enum class EDGOutfitSlot : uint8
{
    Headwear = 0,
    Eyewear = 1,
    Top = 2,
    Outerwear = 3,
    Bottom = 4,
    Socks = 5,
    Footwear = 6,
    Glove = 7,
    Wrist = 8,
    Bag = 9,
    Accessory = 10
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGOutfitVariant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName VariantId = TEXT("Default");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FLinearColor> VectorParameters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, float> ScalarParameters;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGEquippedOutfitEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EDGOutfitSlot Slot = EDGOutfitSlot::Top;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName VariantId = TEXT("Default");
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGOutfitLoadout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FDGEquippedOutfitEntry> Equipped;
};
