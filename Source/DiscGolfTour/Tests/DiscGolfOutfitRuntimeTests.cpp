#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitItem.h"
#include "Engine/StaticMesh.h"

#include "../DiscGolfOutfitRuntime.h"
#include "../DiscGolfSaveGame.h"
#include "../DiscGolfTourGameInstance.h"

namespace
{
    FDGEquippedOutfitEntry MakeEntry(
        EDGOutfitSlot Slot,
        const TCHAR* ItemId,
        const TCHAR* VariantId = TEXT("Default"))
    {
        FDGEquippedOutfitEntry Entry;
        Entry.Slot = Slot;
        Entry.ItemId = FName(ItemId);
        Entry.VariantId = FName(VariantId);
        return Entry;
    }

    UDiscGolfOutfitItem* AddStaticItem(
        UDiscGolfOutfitCatalog& Catalog,
        UStaticMesh& SharedMesh,
        EDGOutfitSlot Slot,
        const TCHAR* ItemId,
        TConstArrayView<EDGOutfitSlot> ConflictingSlots = {})
    {
        UDiscGolfOutfitItem* Item = NewObject<UDiscGolfOutfitItem>(&Catalog);
        Item->ItemId = FName(ItemId);
        Item->DisplayName = FText::FromName(Item->ItemId);
        Item->Slot = Slot;
        Item->StaticMesh = TSoftObjectPtr<UStaticMesh>(&SharedMesh);
        Item->MinHeightCm = 150.0f;
        Item->MaxHeightCm = 210.0f;
        Item->ConflictingSlots.Append(ConflictingSlots);

        FDGOutfitVariant Default;
        Default.VariantId = TEXT("Default");
        Item->Variants.Add(Default);
        FDGOutfitVariant Teal;
        Teal.VariantId = TEXT("Teal");
        Item->Variants.Add(Teal);
        Catalog.Items.Add(TSoftObjectPtr<UDiscGolfOutfitItem>(Item));
        return Item;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfOutfitFrozenSlotOrderTest,
    "DiscGolfTour.Character.Session6.Outfit.FrozenSlotOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfOutfitFrozenSlotOrderTest::RunTest(const FString& Parameters)
{
    const TArray<EDGOutfitSlot>& Slots = DiscGolfOutfitRuntime::GetOrderedSlots();
    TestEqual(TEXT("All framework outfit slots are exposed"), Slots.Num(), 11);
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        TestEqual(
            *FString::Printf(TEXT("Slot %d retains its serialized ordinal"), Index),
            static_cast<int32>(Slots[Index]),
            Index);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfOutfitPersistenceNormalizationTest,
    "DiscGolfTour.Character.Session6.Outfit.PersistenceNormalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfOutfitPersistenceNormalizationTest::RunTest(const FString& Parameters)
{
    FDGOutfitLoadout Input;
    Input.Equipped.Add(MakeEntry(EDGOutfitSlot::Bottom, TEXT("bottom_a")));
    Input.Equipped.Add(MakeEntry(EDGOutfitSlot::Top, TEXT("top_old")));
    Input.Equipped.Add(MakeEntry(EDGOutfitSlot::Accessory, TEXT("")));
    Input.Equipped.Add(MakeEntry(EDGOutfitSlot::Top, TEXT("top_new"), TEXT("Teal")));
    Input.Equipped.Add(MakeEntry(
        static_cast<EDGOutfitSlot>(255), TEXT("invalid_slot")));

    const FDGOutfitLoadout Normal = DiscGolfOutfitRuntime::NormalizeForPersistence(Input);
    TestEqual(TEXT("None entries and duplicate slots collapse"), Normal.Equipped.Num(), 2);
    if (Normal.Equipped.Num() == 2)
    {
        TestTrue(TEXT("Entries use frozen slot ordering"),
            Normal.Equipped[0].Slot == EDGOutfitSlot::Top
            && Normal.Equipped[1].Slot == EDGOutfitSlot::Bottom);
        TestEqual(TEXT("The final selection wins within a slot"),
            Normal.Equipped[0].ItemId, FName(TEXT("top_new")));
        TestEqual(TEXT("Variant IDs remain stable through structural normalization"),
            Normal.Equipped[0].VariantId, FName(TEXT("Teal")));
    }
    TestTrue(TEXT("Equivalent loadouts ignore input ordering and superseded entries"),
        DiscGolfOutfitRuntime::AreLoadoutsEquivalent(Input, Normal));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfOutfitCanonicalResolutionTest,
    "DiscGolfTour.Character.Session6.Outfit.CanonicalResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfOutfitCanonicalResolutionTest::RunTest(const FString& Parameters)
{
    UDiscGolfOutfitCatalog* Catalog = NewObject<UDiscGolfOutfitCatalog>();
    UStaticMesh* SharedMesh = NewObject<UStaticMesh>(Catalog);
    AddStaticItem(*Catalog, *SharedMesh, EDGOutfitSlot::Top, TEXT("top_valid"));
    UDiscGolfOutfitItem* TallOnly = AddStaticItem(
        *Catalog, *SharedMesh, EDGOutfitSlot::Bottom, TEXT("bottom_tall_only"));
    TallOnly->MinHeightCm = 200.0f;

    FDGBodyProfile Body;
    Body.HeightCm = 183.0f;
    FDGOutfitLoadout Requested;
    Requested.Equipped.Add(MakeEntry(
        EDGOutfitSlot::Top, TEXT("top_valid"), TEXT("removed_variant")));
    Requested.Equipped.Add(MakeEntry(
        EDGOutfitSlot::Outerwear, TEXT("deleted_catalog_item")));

    const FDiscGolfOutfitResolution Resolution =
        DiscGolfOutfitRuntime::ResolveCanonicalLoadout(Requested, Catalog, Body);
    TestTrue(TEXT("The catalog is reported available"), Resolution.bCatalogAvailable);
    TestFalse(TEXT("A missing saved item is reported without rejecting valid slots"),
        Resolution.bAllEntriesResolved);
    TestEqual(TEXT("The valid saved item remains equipped"), Resolution.Loadout.Equipped.Num(), 1);
    if (Resolution.Loadout.Equipped.Num() == 1)
    {
        TestEqual(TEXT("A removed variant falls back to the canonical Default ID"),
            Resolution.Loadout.Equipped[0].VariantId, FName(TEXT("Default")));
    }
    TestTrue(TEXT("Missing-item recovery emits actionable status"),
        !Resolution.Warnings.IsEmpty()
        && Resolution.Warnings[0].Contains(TEXT("deleted_catalog_item")));

    const FDiscGolfOutfitResolution MissingCatalog =
        DiscGolfOutfitRuntime::ResolveCanonicalLoadout(Requested, nullptr, Body);
    TestFalse(TEXT("An unavailable catalog is explicit"), MissingCatalog.bCatalogAvailable);
    TestFalse(TEXT("A non-empty request cannot resolve without its catalog"),
        MissingCatalog.bAllEntriesResolved);
    TestTrue(TEXT("Catalog failure resolves safely to an empty visual loadout"),
        MissingCatalog.Loadout.Equipped.IsEmpty());

    const TArray<FDiscGolfOutfitOption> BottomOptions =
        DiscGolfOutfitRuntime::GetOptionsForSlot(Catalog, EDGOutfitSlot::Bottom, Body);
    TestEqual(TEXT("Catalog-backed UI returns the tall-only option"), BottomOptions.Num(), 1);
    if (BottomOptions.Num() == 1)
    {
        TestFalse(TEXT("Body-range incompatible items are disabled"),
            BottomOptions[0].bCompatible);
        TestTrue(TEXT("Disabled items disclose their accepted body range"),
            BottomOptions[0].CompatibilityReason.Contains(TEXT("200-210 cm")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfOutfitConflictSelectionTest,
    "DiscGolfTour.Character.Session6.Outfit.ConflictSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfOutfitConflictSelectionTest::RunTest(const FString& Parameters)
{
    UDiscGolfOutfitCatalog* Catalog = NewObject<UDiscGolfOutfitCatalog>();
    UStaticMesh* SharedMesh = NewObject<UStaticMesh>(Catalog);
    AddStaticItem(*Catalog, *SharedMesh, EDGOutfitSlot::Top, TEXT("top_valid"));
    const EDGOutfitSlot OuterwearConflicts[] = {EDGOutfitSlot::Top};
    AddStaticItem(
        *Catalog,
        *SharedMesh,
        EDGOutfitSlot::Outerwear,
        TEXT("outerwear_wins"),
        OuterwearConflicts);

    FDGBodyProfile Body;
    FDGOutfitLoadout Draft;
    Draft.Equipped.Add(MakeEntry(EDGOutfitSlot::Top, TEXT("top_valid")));
    FString Status;
    const bool bSelected = DiscGolfOutfitRuntime::SetSlotSelection(
        Draft,
        EDGOutfitSlot::Outerwear,
        TEXT("outerwear_wins"),
        TEXT("Teal"),
        Catalog,
        Body,
        Status);

    TestTrue(TEXT("A valid new selection applies"), bSelected);
    TestEqual(TEXT("The new selection removes reciprocal conflicting slots"),
        Draft.Equipped.Num(), 1);
    if (Draft.Equipped.Num() == 1)
    {
        TestTrue(TEXT("The new conflicting item wins deterministically"),
            Draft.Equipped[0].Slot == EDGOutfitSlot::Outerwear
            && Draft.Equipped[0].ItemId == FName(TEXT("outerwear_wins"))
            && Draft.Equipped[0].VariantId == FName(TEXT("Teal")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfOutfitSaveGameSerializationTest,
    "DiscGolfTour.Character.Session6.Outfit.SaveGameSerialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfOutfitSaveGameSerializationTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Source = NewObject<UDiscGolfSaveGame>();
    Source->CharacterProfile.HeightCm = 199.0f;
    Source->OutfitLoadout.Equipped.Add(
        MakeEntry(EDGOutfitSlot::Top, TEXT("proxy_s6_top_tee_01"), TEXT("Teal")));
    Source->OutfitLoadout.Equipped.Add(
        MakeEntry(EDGOutfitSlot::Bag, TEXT("proxy_s6_bag_backpack_01"), TEXT("Graphite")));

    TArray<uint8> Bytes;
    {
        FMemoryWriter Writer(Bytes, true);
        FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        Archive.ArIsSaveGame = true;
        Source->Serialize(Archive);
    }

    UDiscGolfSaveGame* Restored = NewObject<UDiscGolfSaveGame>();
    {
        FMemoryReader Reader(Bytes, true);
        FObjectAndNameAsStringProxyArchive Archive(Reader, false);
        Archive.ArIsSaveGame = true;
        Restored->Serialize(Archive);
    }

    TestEqual(TEXT("The atomic payload retains schema 9"),
        Restored->SaveSchemaVersion, 9);
    TestEqual(TEXT("The character half of the payload persists"),
        Restored->CharacterProfile.HeightCm, 199.0f);
    TestTrue(TEXT("Stable slot/item/variant outfit identities persist"),
        DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Source->OutfitLoadout, Restored->OutfitLoadout));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfOutfitCurrentSchemaNormalizationTest,
    "DiscGolfTour.Character.Session7.Outfit.Schema8Migration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfOutfitCurrentSchemaNormalizationTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Save = NewObject<UDiscGolfSaveGame>();
    Save->SaveSchemaVersion = 8;
    Save->OutfitLoadout.Equipped.Add(
        MakeEntry(EDGOutfitSlot::Bag, TEXT("bag_kept"), TEXT("Graphite")));
    Save->OutfitLoadout.Equipped.Add(
        MakeEntry(EDGOutfitSlot::Top, TEXT("top_old"), TEXT("Default")));
    Save->OutfitLoadout.Equipped.Add(
        MakeEntry(EDGOutfitSlot::Top, TEXT("top_kept"), TEXT("Teal")));

    const DiscGolfProfilePersistence::EMigrationResult Result =
        DiscGolfProfilePersistence::MigrateToCurrent(*Save);
    TestTrue(TEXT("Schema 8 migrates into the complete schema-9 payload"),
        Result == DiscGolfProfilePersistence::EMigrationResult::Migrated);
    TestEqual(TEXT("Schema 8 advances to schema 9"),
        Save->SaveSchemaVersion, 9);
    TestEqual(TEXT("Duplicate current slots collapse"),
        Save->OutfitLoadout.Equipped.Num(), 2);
    if (Save->OutfitLoadout.Equipped.Num() == 2)
    {
        TestTrue(TEXT("Current saves are rewritten in frozen order with last selection winning"),
            Save->OutfitLoadout.Equipped[0].Slot == EDGOutfitSlot::Top
            && Save->OutfitLoadout.Equipped[0].ItemId == FName(TEXT("top_kept"))
            && Save->OutfitLoadout.Equipped[1].Slot == EDGOutfitSlot::Bag);
    }
    return true;
}

#endif
