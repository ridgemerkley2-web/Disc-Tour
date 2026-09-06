#include "DiscGolfCosmeticCatalog.h"

UDiscGolfCosmeticItem* UDiscGolfCosmeticCatalog::FindById(FName ItemId) const
{
    const TSoftObjectPtr<UDiscGolfCosmeticItem>* Found = Items.FindByPredicate(
        [ItemId](const TSoftObjectPtr<UDiscGolfCosmeticItem>& ItemReference)
        {
            const UDiscGolfCosmeticItem* Item = ItemReference.LoadSynchronous();
            return IsValid(Item) && Item->ItemId == ItemId;
        });
    return Found ? Found->LoadSynchronous() : nullptr;
}

TArray<UDiscGolfCosmeticItem*> UDiscGolfCosmeticCatalog::GetByKind(EDGCosmeticKind Kind) const
{
    TArray<UDiscGolfCosmeticItem*> Result;
    for (const TSoftObjectPtr<UDiscGolfCosmeticItem>& ItemReference : Items)
    {
        UDiscGolfCosmeticItem* Item = ItemReference.LoadSynchronous();
        if (IsValid(Item) && Item->Kind == Kind)
        {
            Result.Add(Item);
        }
    }
    return Result;
}
