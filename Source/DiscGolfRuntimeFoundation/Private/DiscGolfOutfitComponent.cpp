#include "DiscGolfOutfitComponent.h"

#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitItem.h"

bool UDiscGolfOutfitComponent::ApplyLoadout(
    const FDGOutfitLoadout& Loadout,
    USkeletalMeshComponent* BodyMesh,
    const FDGBodyProfile& Body)
{
    (void)BodyMesh;
    if (!FMath::IsFinite(Body.HeightCm))
    {
        return false;
    }
    for (const FDGEquippedOutfitEntry& Entry : Loadout.Equipped)
    {
        const UDiscGolfOutfitItem* Item = Catalog ? Catalog->FindItemById(Entry.ItemId) : nullptr;
        FDGOutfitVariant Variant;
        if (!Item || Item->Slot != Entry.Slot || !Item->SupportsHeight(Body.HeightCm)
            || !Item->FindVariant(Entry.VariantId, Variant))
        {
            return false;
        }
    }
    CurrentLoadout = Loadout;
    OnBodyCoverageChanged.Broadcast(GetCoveredBodyRegions());
    return true;
}

void UDiscGolfOutfitComponent::ClearOutfit()
{
    CurrentLoadout.Equipped.Reset();
    OnBodyCoverageChanged.Broadcast(GetCoveredBodyRegions());
}

TArray<EDGBodyRegion> UDiscGolfOutfitComponent::GetCoveredBodyRegions() const
{
    TArray<EDGBodyRegion> Result;
    if (!Catalog)
    {
        return Result;
    }
    for (const FDGEquippedOutfitEntry& Entry : CurrentLoadout.Equipped)
    {
        if (const UDiscGolfOutfitItem* Item = Catalog->FindItemById(Entry.ItemId))
        {
            for (EDGBodyRegion Region : Item->CoveredBodyRegions)
            {
                Result.AddUnique(Region);
            }
        }
    }
    return Result;
}

void UDiscGolfOutfitComponent::ReapplyBodyMorphs(const FDGBodyProfile& Body)
{
    (void)Body;
    OnBodyCoverageChanged.Broadcast(GetCoveredBodyRegions());
}
