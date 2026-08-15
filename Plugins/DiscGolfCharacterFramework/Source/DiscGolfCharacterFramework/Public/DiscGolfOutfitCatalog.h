#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfOutfitCatalog.generated.h"

class UDiscGolfOutfitItem;

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfOutfitCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Outfit")
    TArray<TSoftObjectPtr<UDiscGolfOutfitItem>> Items;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Outfit")
    UDiscGolfOutfitItem* FindItemById(FName ItemId) const;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Outfit")
    TArray<UDiscGolfOutfitItem*> GetItemsForSlot(EDGOutfitSlot Slot) const;
};
