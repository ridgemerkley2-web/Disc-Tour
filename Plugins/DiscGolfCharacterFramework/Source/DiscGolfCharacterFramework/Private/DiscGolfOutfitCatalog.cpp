#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitItem.h"

UDiscGolfOutfitItem* UDiscGolfOutfitCatalog::FindItemById(FName ItemId) const
{
    if (ItemId.IsNone())
    {
        return nullptr;
    }

    for (const TSoftObjectPtr<UDiscGolfOutfitItem>& SoftItem : Items)
    {
        if (UDiscGolfOutfitItem* Item = SoftItem.LoadSynchronous())
        {
            if (Item->ItemId == ItemId)
            {
                return Item;
            }
        }
    }

    return nullptr;
}

TArray<UDiscGolfOutfitItem*> UDiscGolfOutfitCatalog::GetItemsForSlot(EDGOutfitSlot Slot) const
{
    TArray<UDiscGolfOutfitItem*> Result;

    for (const TSoftObjectPtr<UDiscGolfOutfitItem>& SoftItem : Items)
    {
        if (UDiscGolfOutfitItem* Item = SoftItem.LoadSynchronous())
        {
            if (Item->Slot == Slot)
            {
                Result.Add(Item);
            }
        }
    }

    return Result;
}
