#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfOutfitItem.h"
#include "DiscGolfOutfitCatalog.generated.h"

UCLASS(BlueprintType)
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfOutfitCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TSoftObjectPtr<UDiscGolfOutfitItem>> Items;

    UFUNCTION(BlueprintPure) UDiscGolfOutfitItem* FindItemById(FName ItemId) const;
    UFUNCTION(BlueprintPure) TArray<UDiscGolfOutfitItem*> GetItemsForSlot(EDGOutfitSlot Slot) const;
};
