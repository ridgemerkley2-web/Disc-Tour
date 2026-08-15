#include "DiscGolfOutfitItem.h"

bool UDiscGolfOutfitItem::FindVariant(FName VariantId, FDGOutfitVariant& OutVariant) const
{
    for (const FDGOutfitVariant& Variant : Variants)
    {
        if (Variant.VariantId == VariantId)
        {
            OutVariant = Variant;
            return true;
        }
    }

    for (const FDGOutfitVariant& Variant : Variants)
    {
        if (Variant.VariantId == TEXT("Default"))
        {
            OutVariant = Variant;
            return true;
        }
    }

    return false;
}
