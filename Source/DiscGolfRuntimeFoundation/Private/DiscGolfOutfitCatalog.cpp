#include "DiscGolfOutfitCatalog.h"

UDiscGolfOutfitItem* UDiscGolfOutfitCatalog::FindItemById(FName ItemId) const
{
    const TSoftObjectPtr<UDiscGolfOutfitItem>* Found = Items.FindByPredicate(
        [ItemId](const TSoftObjectPtr<UDiscGolfOutfitItem>& ItemReference)
        {
            const UDiscGolfOutfitItem* Item = ItemReference.LoadSynchronous();
            return IsValid(Item) && Item->ItemId == ItemId;
        });
    return Found ? Found->LoadSynchronous() : nullptr;
}

TArray<UDiscGolfOutfitItem*> UDiscGolfOutfitCatalog::GetItemsForSlot(EDGOutfitSlot Slot) const
{
    TArray<UDiscGolfOutfitItem*> Result;
    for (const TSoftObjectPtr<UDiscGolfOutfitItem>& ItemReference : Items)
    {
        UDiscGolfOutfitItem* Item = ItemReference.LoadSynchronous();
        if (IsValid(Item) && Item->Slot == Slot)
        {
            Result.Add(Item);
        }
    }
    return Result;
}
