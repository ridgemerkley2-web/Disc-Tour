#include "DiscGolfCosmeticCatalog.h"
#include "DiscGolfCosmeticItem.h"

UDiscGolfCosmeticItem* UDiscGolfCosmeticCatalog::FindById(FName ItemId) const
{
    if (ItemId.IsNone())
    {
        return nullptr;
    }

    for (const TSoftObjectPtr<UDiscGolfCosmeticItem>& SoftItem : Items)
    {
        if (UDiscGolfCosmeticItem* Item = SoftItem.LoadSynchronous())
        {
            if (Item->ItemId == ItemId)
            {
                return Item;
            }
        }
    }

    return nullptr;
}

TArray<UDiscGolfCosmeticItem*> UDiscGolfCosmeticCatalog::GetByKind(EDGCosmeticKind Kind) const
{
    TArray<UDiscGolfCosmeticItem*> Result;

    for (const TSoftObjectPtr<UDiscGolfCosmeticItem>& SoftItem : Items)
    {
        if (UDiscGolfCosmeticItem* Item = SoftItem.LoadSynchronous())
        {
            if (Item->Kind == Kind)
            {
                Result.Add(Item);
            }
        }
    }

    return Result;
}
