#include "DiscGolfOutfitRuntime.h"

#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitItem.h"

namespace
{
    bool SlotsConflict(
        const UDiscGolfOutfitItem* A,
        const UDiscGolfOutfitItem* B)
    {
        return A && B
            && (A->ConflictingSlots.Contains(B->Slot)
                || B->ConflictingSlots.Contains(A->Slot));
    }

    UDiscGolfOutfitItem* FindCatalogItem(
        const UDiscGolfOutfitCatalog* Catalog,
        FName ItemId)
    {
        return Catalog ? Catalog->FindItemById(ItemId) : nullptr;
    }

    FString JoinWarnings(const TArray<FString>& Warnings)
    {
        return Warnings.IsEmpty()
            ? FString()
            : FString::Join(Warnings, TEXT(" "));
    }
}

const TArray<EDGOutfitSlot>& DiscGolfOutfitRuntime::GetOrderedSlots()
{
    static const TArray<EDGOutfitSlot> Slots = {
        EDGOutfitSlot::Headwear,
        EDGOutfitSlot::Eyewear,
        EDGOutfitSlot::Top,
        EDGOutfitSlot::Outerwear,
        EDGOutfitSlot::Bottom,
        EDGOutfitSlot::Socks,
        EDGOutfitSlot::Footwear,
        EDGOutfitSlot::Glove,
        EDGOutfitSlot::Wrist,
        EDGOutfitSlot::Bag,
        EDGOutfitSlot::Accessory
    };

    static_assert(static_cast<uint8>(EDGOutfitSlot::Headwear) == 0);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Eyewear) == 1);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Top) == 2);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Outerwear) == 3);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Bottom) == 4);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Socks) == 5);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Footwear) == 6);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Glove) == 7);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Wrist) == 8);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Bag) == 9);
    static_assert(static_cast<uint8>(EDGOutfitSlot::Accessory) == 10);
    return Slots;
}

FText DiscGolfOutfitRuntime::GetSlotDisplayName(EDGOutfitSlot Slot)
{
    switch (Slot)
    {
        case EDGOutfitSlot::Headwear: return FText::FromString(TEXT("HEADWEAR"));
        case EDGOutfitSlot::Eyewear: return FText::FromString(TEXT("EYEWEAR"));
        case EDGOutfitSlot::Top: return FText::FromString(TEXT("TOP"));
        case EDGOutfitSlot::Outerwear: return FText::FromString(TEXT("OUTERWEAR"));
        case EDGOutfitSlot::Bottom: return FText::FromString(TEXT("BOTTOM"));
        case EDGOutfitSlot::Socks: return FText::FromString(TEXT("SOCKS"));
        case EDGOutfitSlot::Footwear: return FText::FromString(TEXT("FOOTWEAR"));
        case EDGOutfitSlot::Glove: return FText::FromString(TEXT("GLOVE"));
        case EDGOutfitSlot::Wrist: return FText::FromString(TEXT("WRIST"));
        case EDGOutfitSlot::Bag: return FText::FromString(TEXT("DISC BAG"));
        case EDGOutfitSlot::Accessory: return FText::FromString(TEXT("ACCESSORY"));
        default: return FText::FromString(TEXT("OUTFIT"));
    }
}

const FDGEquippedOutfitEntry* DiscGolfOutfitRuntime::FindEntryForSlot(
    const FDGOutfitLoadout& Loadout,
    EDGOutfitSlot Slot)
{
    return Loadout.Equipped.FindByPredicate(
        [Slot](const FDGEquippedOutfitEntry& Entry)
        {
            return Entry.Slot == Slot;
        });
}

FDGOutfitLoadout DiscGolfOutfitRuntime::NormalizeForPersistence(
    const FDGOutfitLoadout& Loadout)
{
    TMap<EDGOutfitSlot, FDGEquippedOutfitEntry> LastEntryBySlot;
    for (const FDGEquippedOutfitEntry& Entry : Loadout.Equipped)
    {
        if (!Entry.ItemId.IsNone())
        {
            LastEntryBySlot.Add(Entry.Slot, Entry);
        }
    }

    FDGOutfitLoadout Result;
    for (EDGOutfitSlot Slot : GetOrderedSlots())
    {
        if (const FDGEquippedOutfitEntry* Entry = LastEntryBySlot.Find(Slot))
        {
            Result.Equipped.Add(*Entry);
        }
    }
    return Result;
}

FDiscGolfOutfitResolution DiscGolfOutfitRuntime::ResolveCanonicalLoadout(
    const FDGOutfitLoadout& Requested,
    const UDiscGolfOutfitCatalog* Catalog,
    const FDGBodyProfile& BodyProfile)
{
    FDiscGolfOutfitResolution Result;
    Result.bCatalogAvailable = Catalog != nullptr;
    for (const FDGEquippedOutfitEntry& Entry : Requested.Equipped)
    {
        if (!Entry.ItemId.IsNone() && !GetOrderedSlots().Contains(Entry.Slot))
        {
            Result.bAllEntriesResolved = false;
            Result.Warnings.Add(TEXT("An outfit entry used an unknown slot and resolved to None."));
        }
    }
    const FDGOutfitLoadout Structural = NormalizeForPersistence(Requested);

    if (!Catalog)
    {
        Result.bAllEntriesResolved = Result.bAllEntriesResolved
            && Structural.Equipped.IsEmpty();
        if (!Structural.Equipped.IsEmpty())
        {
            Result.Warnings.Add(TEXT("Outfit catalog is unavailable; saved cosmetics were skipped."));
        }
        return Result;
    }

    TArray<UDiscGolfOutfitItem*> AcceptedItems;
    for (const FDGEquippedOutfitEntry& Entry : Structural.Equipped)
    {
        UDiscGolfOutfitItem* Item = FindCatalogItem(Catalog, Entry.ItemId);
        if (!Item)
        {
            Result.bAllEntriesResolved = false;
            Result.Warnings.Add(FString::Printf(
                TEXT("Missing outfit item '%s' resolved to None."),
                *Entry.ItemId.ToString()));
            continue;
        }
        if (Item->ItemId.IsNone() || Item->Slot != Entry.Slot)
        {
            Result.bAllEntriesResolved = false;
            Result.Warnings.Add(FString::Printf(
                TEXT("Outfit item '%s' has an invalid saved slot and was skipped."),
                *Entry.ItemId.ToString()));
            continue;
        }
        if (!Item->SupportsHeight(BodyProfile.HeightCm))
        {
            Result.bAllEntriesResolved = false;
            Result.Warnings.Add(FString::Printf(
                TEXT("Outfit item '%s' is incompatible with %.1f cm and resolved to None."),
                *Entry.ItemId.ToString(), BodyProfile.HeightCm));
            continue;
        }

        const bool bHasSkeletalMesh = !Item->SkeletalMesh.IsNull();
        const bool bHasStaticMesh = !Item->StaticMesh.IsNull();
        if (bHasSkeletalMesh == bHasStaticMesh)
        {
            Result.bAllEntriesResolved = false;
            Result.Warnings.Add(FString::Printf(
                TEXT("Outfit item '%s' must define exactly one mesh and was skipped."),
                *Entry.ItemId.ToString()));
            continue;
        }

        bool bConflicts = false;
        for (const UDiscGolfOutfitItem* Accepted : AcceptedItems)
        {
            if (SlotsConflict(Item, Accepted))
            {
                bConflicts = true;
                break;
            }
        }
        if (bConflicts)
        {
            Result.bAllEntriesResolved = false;
            Result.Warnings.Add(FString::Printf(
                TEXT("Outfit item '%s' conflicted with an earlier slot and resolved to None."),
                *Entry.ItemId.ToString()));
            continue;
        }

        FDGOutfitVariant Variant;
        if (!Item->FindVariant(Entry.VariantId, Variant))
        {
            Result.bAllEntriesResolved = false;
            Result.Warnings.Add(FString::Printf(
                TEXT("Outfit item '%s' has no usable Default variant and was skipped."),
                *Entry.ItemId.ToString()));
            continue;
        }

        FDGEquippedOutfitEntry Canonical;
        Canonical.Slot = Item->Slot;
        Canonical.ItemId = Item->ItemId;
        Canonical.VariantId = Variant.VariantId;
        Result.Loadout.Equipped.Add(Canonical);
        AcceptedItems.Add(Item);
    }

    Result.Loadout = NormalizeForPersistence(Result.Loadout);
    return Result;
}

bool DiscGolfOutfitRuntime::SetSlotSelection(
    FDGOutfitLoadout& InOutLoadout,
    EDGOutfitSlot Slot,
    FName ItemId,
    FName VariantId,
    const UDiscGolfOutfitCatalog* Catalog,
    const FDGBodyProfile& BodyProfile,
    FString& OutStatus)
{
    if (!GetOrderedSlots().Contains(Slot))
    {
        OutStatus = TEXT("That outfit slot is not part of the frozen Session 6 schema.");
        return false;
    }

    FDGOutfitLoadout Candidate = NormalizeForPersistence(InOutLoadout);
    Candidate.Equipped.RemoveAll(
        [Slot](const FDGEquippedOutfitEntry& Entry)
        {
            return Entry.Slot == Slot;
        });

    if (ItemId.IsNone())
    {
        InOutLoadout = NormalizeForPersistence(Candidate);
        OutStatus = FString::Printf(TEXT("%s set to None."), *GetSlotDisplayName(Slot).ToString());
        return true;
    }

    UDiscGolfOutfitItem* NewItem = FindCatalogItem(Catalog, ItemId);
    if (!NewItem || NewItem->Slot != Slot || !NewItem->SupportsHeight(BodyProfile.HeightCm))
    {
        OutStatus = TEXT("That outfit item is unavailable or incompatible; the draft was retained.");
        return false;
    }

    Candidate.Equipped.RemoveAll(
        [Catalog, NewItem](const FDGEquippedOutfitEntry& Entry)
        {
            return SlotsConflict(NewItem, FindCatalogItem(Catalog, Entry.ItemId));
        });

    FDGEquippedOutfitEntry NewEntry;
    NewEntry.Slot = Slot;
    NewEntry.ItemId = ItemId;
    NewEntry.VariantId = VariantId;
    Candidate.Equipped.Add(NewEntry);

    const FDiscGolfOutfitResolution Resolution = ResolveCanonicalLoadout(
        Candidate, Catalog, BodyProfile);
    if (!Resolution.bAllEntriesResolved)
    {
        OutStatus = JoinWarnings(Resolution.Warnings);
        return false;
    }

    InOutLoadout = Resolution.Loadout;
    OutStatus = FString::Printf(TEXT("%s previewed in %s."),
        *NewItem->DisplayName.ToString(), *GetSlotDisplayName(Slot).ToString());
    return true;
}

bool DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
    const FDGOutfitLoadout& A,
    const FDGOutfitLoadout& B)
{
    const FDGOutfitLoadout NormalA = NormalizeForPersistence(A);
    const FDGOutfitLoadout NormalB = NormalizeForPersistence(B);
    if (NormalA.Equipped.Num() != NormalB.Equipped.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < NormalA.Equipped.Num(); ++Index)
    {
        const FDGEquippedOutfitEntry& EntryA = NormalA.Equipped[Index];
        const FDGEquippedOutfitEntry& EntryB = NormalB.Equipped[Index];
        if (EntryA.Slot != EntryB.Slot
            || EntryA.ItemId != EntryB.ItemId
            || EntryA.VariantId != EntryB.VariantId)
        {
            return false;
        }
    }
    return true;
}

TArray<FDiscGolfOutfitOption> DiscGolfOutfitRuntime::GetOptionsForSlot(
    const UDiscGolfOutfitCatalog* Catalog,
    EDGOutfitSlot Slot,
    const FDGBodyProfile& BodyProfile)
{
    TArray<FDiscGolfOutfitOption> Result;
    if (!Catalog)
    {
        return Result;
    }

    for (UDiscGolfOutfitItem* Item : Catalog->GetItemsForSlot(Slot))
    {
        if (!Item || Item->ItemId.IsNone())
        {
            continue;
        }

        FDiscGolfOutfitOption& Option = Result.AddDefaulted_GetRef();
        Option.ItemId = Item->ItemId;
        Option.DisplayName = Item->DisplayName.IsEmpty()
            ? FText::FromName(Item->ItemId) : Item->DisplayName;
        Option.Slot = Item->Slot;
        for (const FDGOutfitVariant& Variant : Item->Variants)
        {
            if (!Variant.VariantId.IsNone())
            {
                Option.VariantIds.AddUnique(Variant.VariantId);
            }
        }

        const bool bHasSkeletalMesh = !Item->SkeletalMesh.IsNull();
        const bool bHasStaticMesh = !Item->StaticMesh.IsNull();
        const bool bMeshDefinitionValid = bHasSkeletalMesh != bHasStaticMesh;
        const bool bHeightCompatible = Item->SupportsHeight(BodyProfile.HeightCm);
        Option.bCompatible = bMeshDefinitionValid
            && bHeightCompatible
            && !Option.VariantIds.IsEmpty();
        if (!bHeightCompatible)
        {
            Option.CompatibilityReason = FString::Printf(
                TEXT("Supports %.0f-%.0f cm"), Item->MinHeightCm, Item->MaxHeightCm);
        }
        else if (!bMeshDefinitionValid)
        {
            Option.CompatibilityReason = TEXT("Invalid proxy mesh definition");
        }
        else if (Option.VariantIds.IsEmpty())
        {
            Option.CompatibilityReason = TEXT("No usable material variants");
        }
    }

    Result.Sort([](const FDiscGolfOutfitOption& A, const FDiscGolfOutfitOption& B)
    {
        const int32 NameOrder = A.DisplayName.ToString().Compare(B.DisplayName.ToString());
        return NameOrder == 0
            ? A.ItemId.LexicalLess(B.ItemId)
            : NameOrder < 0;
    });
    return Result;
}
