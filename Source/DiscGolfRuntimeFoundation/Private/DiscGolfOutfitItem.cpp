#include "DiscGolfOutfitItem.h"

bool UDiscGolfOutfitItem::SupportsHeight(float HeightCm) const
{
    return FMath::IsFinite(HeightCm) && HeightCm >= MinHeightCm && HeightCm <= MaxHeightCm;
}

bool UDiscGolfOutfitItem::FindVariant(FName VariantId, FDGOutfitVariant& OutVariant) const
{
    const FDGOutfitVariant* Found = Variants.FindByPredicate(
        [VariantId](const FDGOutfitVariant& Variant)
        {
            return !Variant.VariantId.IsNone() && Variant.VariantId == VariantId;
        });
    if (!Found)
    {
        Found = Variants.FindByPredicate([](const FDGOutfitVariant& Variant)
        {
            return Variant.VariantId == FName(TEXT("Default"));
        });
    }
    if (!Found)
    {
        return false;
    }
    OutVariant = *Found;
    return true;
}
