#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"

#include "DiscGolfCosmeticCatalog.h"
#include "DiscGolfCosmeticItem.h"

#include "../DiscGolfFullCharacterRuntime.h"
#include "../DiscGolfOutfitRuntime.h"
#include "../DiscGolfSaveGame.h"
#include "../DiscGolfTourGameInstance.h"

#include <limits>

namespace
{
bool Session7NearlyEqual(float A, float B)
{
    return FMath::IsNearlyEqual(A, B, 1.0e-5f);
}

FDGEquippedOutfitEntry MakeOutfitEntry(
    EDGOutfitSlot Slot,
    const TCHAR* ItemId,
    const TCHAR* VariantId)
{
    FDGEquippedOutfitEntry Entry;
    Entry.Slot = Slot;
    Entry.ItemId = FName(ItemId);
    Entry.VariantId = FName(VariantId);
    return Entry;
}

UDiscGolfCosmeticItem* AddCosmetic(
    UDiscGolfCosmeticCatalog& Catalog,
    EDGCosmeticKind Kind,
    const TCHAR* ItemId)
{
    UDiscGolfCosmeticItem* Item = NewObject<UDiscGolfCosmeticItem>(&Catalog);
    Item->Kind = Kind;
    Item->ItemId = FName(ItemId);
    Item->DisplayName = FText::FromName(Item->ItemId);
    Catalog.Items.Add(TSoftObjectPtr<UDiscGolfCosmeticItem>(Item));
    return Item;
}

FDGFullCharacterCustomization MakeRoundTripFixture()
{
    FDGFullCharacterCustomization Result =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    Result.AvatarBackendId = TEXT("metahuman_assembled");
    Result.Identity.DisplayName = TEXT("Session Seven Player");
    Result.Identity.Handedness = EDGHandedness::Left;
    Result.Identity.VoiceId = TEXT("voice_alt");
    Result.Identity.PronounSetId = TEXT("pronouns_alt");
    Result.Body.HeightCm = 201.0f;
    Result.Body.WingspanScale = 1.06f;
    Result.Body.ShoulderWidthScale = 0.95f;
    Result.Body.TorsoLengthScale = 1.04f;
    Result.Body.LegLengthScale = 0.96f;
    Result.Body.HandScale = 1.05f;
    Result.Body.MassKg = 97.0f;
    Result.BodyBuild.Muscularity = 0.72f;
    Result.BodyBuild.BodyFat = 0.18f;
    Result.BodyBuild.Chest = 0.40f;
    Result.BodyBuild.Waist = -0.30f;
    Result.BodyBuild.Hips = 0.20f;
    Result.BodyBuild.Arms = 0.55f;
    Result.BodyBuild.Legs = -0.45f;
    DiscGolfFullCharacterRuntime::ApplyFacePreset(TEXT("face_square"), Result.Face);
    Result.Face.MorphValues[TEXT("eye_size")] = 0.37f;
    Result.Face.MorphValues[TEXT("ear_angle")] = -0.41f;
    Result.Hair.HairStyleId = TEXT("hair_medium");
    Result.Hair.FacialHairId = TEXT("facialhair_beard");
    Result.Hair.EyebrowId = TEXT("brow_alt");
    Result.Hair.HairColor = FLinearColor(0.13f, 0.07f, 0.035f, 1.0f);
    Result.Hair.FacialHairColor = FLinearColor(0.11f, 0.06f, 0.03f, 1.0f);
    Result.Hair.EyebrowColor = FLinearColor(0.09f, 0.05f, 0.025f, 1.0f);
    Result.Appearance.SkinTone = FLinearColor(0.68f, 0.47f, 0.34f, 1.0f);
    Result.Appearance.EyeColor = FLinearColor(0.08f, 0.18f, 0.26f, 1.0f);
    Result.Appearance.Complexion = 0.61f;
    Result.Appearance.Freckles = 0.42f;
    Result.Appearance.SunExposure = 0.73f;
    Result.Appearance.ScarId = TEXT("scar_proxy_01");
    Result.Appearance.TattooIds = {TEXT("tattoo_proxy_01")};
    Result.ThrowStyle.RunUpIntensity = 0.91f;
    Result.ThrowStyle.ReachBackAmount = 0.87f;
    Result.ThrowStyle.TorsoRotation = 0.62f;
    Result.ThrowStyle.BraceIntensity = 0.78f;
    Result.ThrowStyle.Explosiveness = 0.69f;
    Result.ThrowStyle.FollowThrough = 0.94f;
    Result.ThrowStyle.PowerMultiplier = 1.24f;
    Result.ThrowStyle.SpinMultiplier = 0.76f;
    Result.Outfit.Equipped.Add(MakeOutfitEntry(
        EDGOutfitSlot::Top, TEXT("proxy_s6_top_tee_01"), TEXT("Teal")));
    Result.Outfit.Equipped.Add(MakeOutfitEntry(
        EDGOutfitSlot::Bag, TEXT("proxy_s6_bag_backpack_01"), TEXT("Graphite")));
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Result);
    return Result;
}

class FScopedFullCharacterSaveSlotCleanup
{
public:
    explicit FScopedFullCharacterSaveSlotCleanup(FString InSlotName)
        : SlotName(MoveTemp(InSlotName))
    {
    }

    ~FScopedFullCharacterSaveSlotCleanup()
    {
        UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    }

private:
    FString SlotName;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFullCharacterContractTest,
    "DiscGolfTour.Character.Session7.FullCharacter.ContractNormalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFullCharacterContractTest::RunTest(const FString& Parameters)
{
    const FDGFullCharacterCustomization SafeFreshInstall =
        DiscGolfFullCharacterRuntime::MakeFreshInstallCustomization(false);
    const FDGFullCharacterCustomization ReleaseFreshInstall =
        DiscGolfFullCharacterRuntime::MakeFreshInstallCustomization(true);
    TestEqual(TEXT("A non-release fresh install retains the stable DGMaster default"),
        SafeFreshInstall.AvatarBackendId, FName(TEXT("dg_master")));
    TestEqual(TEXT("A release fresh install requests the retained MetaHuman backend"),
        ReleaseFreshInstall.AvatarBackendId, FName(TEXT("metahuman_assembled")));

    FDGFullCharacterCustomization Character;
    Character.AvatarBackendId = TEXT("unknown_backend");
    Character.Identity.DisplayName =
        TEXT("  Player\x2028\tName\r\nWith A Deliberately Overlong Suffix  ");
    Character.Identity.VoiceId = NAME_None;
    Character.Identity.PronounSetId = NAME_None;
    Character.Body.HeightCm = std::numeric_limits<float>::quiet_NaN();
    Character.BodyBuild.Muscularity = 5.0f;
    Character.Face.PresetId = TEXT("missing_preset");
    Character.Face.MorphValues.Add(TEXT("head_width"), 7.0f);
    Character.Face.MorphValues.Add(
        TEXT("brow_height"), std::numeric_limits<float>::quiet_NaN());
    Character.Face.MorphValues.Add(TEXT("unknown_morph"), 0.5f);
    Character.Appearance.Complexion = -4.0f;
    Character.Appearance.Freckles = 4.0f;
    Character.Appearance.TattooIds = {
        TEXT("tattoo_z"), TEXT("tattoo_none"), TEXT("tattoo_a"),
        TEXT("tattoo_z"), NAME_None
    };
    Character.ThrowStyle.PowerMultiplier = 1.25f;
    Character.ThrowStyle.SpinMultiplier = 0.75f;

    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Character);

    TestEqual(TEXT("Unknown backend IDs fail closed to DGMaster"),
        Character.AvatarBackendId, FName(TEXT("dg_master")));
    FDGFullCharacterCustomization MetaHumanCharacter = Character;
    MetaHumanCharacter.AvatarBackendId = TEXT("metahuman_assembled");
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(MetaHumanCharacter);
    TestEqual(TEXT("The canonical MetaHuman request survives normalization"),
        MetaHumanCharacter.AvatarBackendId,
        FName(TEXT("metahuman_assembled")));
    const FDiscGolfFullCustomizationResolution RuntimeResolved =
        DiscGolfFullCharacterRuntime::ResolveForRuntime(
            MetaHumanCharacter, nullptr, nullptr);
    TestEqual(TEXT("Transient content resolution does not rewrite the backend request"),
        RuntimeResolved.Character.AvatarBackendId,
        FName(TEXT("metahuman_assembled")));
    TestFalse(TEXT("Backend selection participates in full-character equivalence"),
        DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Character, MetaHumanCharacter));
    TestTrue(TEXT("Display names strip controls and retain printable text"),
        !Character.Identity.DisplayName.Contains(TEXT("\t"))
        && !Character.Identity.DisplayName.Contains(TEXT("\r"))
        && !Character.Identity.DisplayName.Contains(TEXT("\n"))
        && !Character.Identity.DisplayName.Contains(TEXT("\x2028"))
        && Character.Identity.DisplayName.Len()
            <= DiscGolfFullCharacterRuntime::MaximumDisplayNameLength);
    TestEqual(TEXT("Missing voice IDs normalize to the stable default"),
        Character.Identity.VoiceId, FName(TEXT("voice_default")));
    TestEqual(TEXT("Missing pronoun IDs normalize to the stable default"),
        Character.Identity.PronounSetId, FName(TEXT("pronouns_default")));
    TestTrue(TEXT("Body and body-build values use the accepted schema bounds"),
        Session7NearlyEqual(Character.Body.HeightCm, 183.0f)
        && Session7NearlyEqual(Character.BodyBuild.Muscularity, 1.0f));
    TestEqual(TEXT("Every persisted face has the complete twenty-key contract"),
        Character.Face.MorphValues.Num(), 20);
    for (FName Key : DiscGolfFullCharacterRuntime::GetFaceMorphKeys())
    {
        TestTrue(*FString::Printf(TEXT("Face key %s is always present"), *Key.ToString()),
            Character.Face.MorphValues.Contains(Key));
    }
    TestEqual(TEXT("The honest proxy-visible face ledger remains exactly five keys"),
        DiscGolfFullCharacterRuntime::GetVisibleProxyFaceMorphKeys().Num(), 5);
    const TArray<FName> ExpectedVisibleMorphs = {
        TEXT("head_width"), TEXT("head_height"), TEXT("cheek_fullness"),
        TEXT("jaw_width"), TEXT("chin_length")
    };
    const TArray<FName>& ActualVisibleMorphs =
        DiscGolfFullCharacterRuntime::GetVisibleProxyFaceMorphKeys();
    for (int32 Index = 0;
        Index < FMath::Min(ExpectedVisibleMorphs.Num(), ActualVisibleMorphs.Num());
        ++Index)
    {
        TestEqual(*FString::Printf(
            TEXT("Visible proxy morph %d retains its frozen identity"), Index),
            ActualVisibleMorphs[Index],
            ExpectedVisibleMorphs[Index]);
    }
    TestTrue(TEXT("Known morphs clamp, non-finite morphs reset, and unknown keys drop"),
        Session7NearlyEqual(Character.Face.MorphValues.FindRef(TEXT("head_width")), 1.0f)
        && Session7NearlyEqual(Character.Face.MorphValues.FindRef(TEXT("brow_height")), 0.0f)
        && !Character.Face.MorphValues.Contains(TEXT("unknown_morph")));
    TestTrue(TEXT("Appearance scalars clamp to presentation-only bounds"),
        Session7NearlyEqual(Character.Appearance.Complexion, 0.0f)
        && Session7NearlyEqual(Character.Appearance.Freckles, 1.0f));
    TestEqual(TEXT("Tattoo none, duplicates and empty IDs canonicalize away"),
        Character.Appearance.TattooIds.Num(), 2);
    if (Character.Appearance.TattooIds.Num() == 2)
    {
        TestTrue(TEXT("Tattoo stable IDs use deterministic lexical ordering"),
            Character.Appearance.TattooIds[0] == FName(TEXT("tattoo_a"))
            && Character.Appearance.TattooIds[1] == FName(TEXT("tattoo_z")));
    }
    TestTrue(TEXT("Full-character persistence cannot become power or spin authority"),
        Session7NearlyEqual(Character.ThrowStyle.PowerMultiplier, 1.0f)
        && Session7NearlyEqual(Character.ThrowStyle.SpinMultiplier, 1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFullCharacterPresetAndRandomizeTest,
    "DiscGolfTour.Character.Session7.FullCharacter.PresetsAndRandomizeLocks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFullCharacterPresetAndRandomizeTest::RunTest(const FString& Parameters)
{
    for (FName Preset : {
        FName(TEXT("face_default")), FName(TEXT("face_square")),
        FName(TEXT("face_narrow")), FName(TEXT("face_round"))})
    {
        FDGFaceProfile Face;
        TestTrue(*FString::Printf(TEXT("Preset %s is available"), *Preset.ToString()),
            DiscGolfFullCharacterRuntime::ApplyFacePreset(Preset, Face));
        TestEqual(TEXT("Every preset initializes all twenty editable values"),
            Face.MorphValues.Num(), 20);
    }

    FDGFaceProfile Untouched;
    Untouched.PresetId = TEXT("face_round");
    Untouched.MorphValues.Add(TEXT("head_width"), 0.91f);
    TestFalse(TEXT("Unknown presets are rejected"),
        DiscGolfFullCharacterRuntime::ApplyFacePreset(
            TEXT("face_missing"), Untouched));
    TestTrue(TEXT("A rejected preset does not mutate the current face"),
        Untouched.PresetId == FName(TEXT("face_round"))
        && Session7NearlyEqual(Untouched.MorphValues.FindRef(TEXT("head_width")), 0.91f));

    UDiscGolfCosmeticCatalog* Catalog = NewObject<UDiscGolfCosmeticCatalog>();
    AddCosmetic(*Catalog, EDGCosmeticKind::Voice, TEXT("voice_catalog_only"));
    AddCosmetic(*Catalog, EDGCosmeticKind::PronounSet, TEXT("pronouns_catalog_only"));

    FDGFullCharacterCustomization Current = MakeRoundTripFixture();
    Current.Identity.DisplayName = TEXT("Locked Name");
    FDiscGolfCharacterRandomizeLocks Locks;
    Locks.bBody = true;
    Locks.bFace = true;
    Locks.bHair = true;
    Locks.bAppearance = true;
    Locks.bThrowStyle = true;
    Locks.bOutfit = true;
    FRandomStream Random(74017);
    const FDGFullCharacterCustomization IdentityRandomized =
        DiscGolfFullCharacterRuntime::Randomize(
            Current, Locks, Catalog, nullptr, Random);
    TestEqual(TEXT("Identity randomization uses a valid catalog voice"),
        IdentityRandomized.Identity.VoiceId,
        FName(TEXT("voice_catalog_only")));
    TestEqual(TEXT("Identity randomization uses a valid catalog pronoun set"),
        IdentityRandomized.Identity.PronounSetId,
        FName(TEXT("pronouns_catalog_only")));
    TestEqual(TEXT("Identity randomization does not invent a display name"),
        IdentityRandomized.Identity.DisplayName, FString(TEXT("Locked Name")));
    TestEqual(TEXT("Randomization preserves the requested avatar backend"),
        IdentityRandomized.AvatarBackendId, Current.AvatarBackendId);
    TestTrue(TEXT("All locked non-identity categories are retained exactly"),
        Session7NearlyEqual(IdentityRandomized.Body.HeightCm, Current.Body.HeightCm)
        && IdentityRandomized.Face.PresetId == Current.Face.PresetId
        && IdentityRandomized.Hair.HairStyleId == Current.Hair.HairStyleId
        && IdentityRandomized.Appearance.ScarId == Current.Appearance.ScarId
        && Session7NearlyEqual(
            IdentityRandomized.ThrowStyle.ReachBackAmount,
            Current.ThrowStyle.ReachBackAmount)
        && DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            IdentityRandomized.Outfit, Current.Outfit));

    Locks.bIdentity = true;
    FRandomStream LockedRandom(74018);
    const FDGFullCharacterCustomization AllLocked =
        DiscGolfFullCharacterRuntime::Randomize(
            Current, Locks, nullptr, nullptr, LockedRandom);
    TestTrue(TEXT("Every category lock is an exact normalized draft-data promise"),
        DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Current, AllLocked));

    FDiscGolfCharacterRandomizeLocks OutfitLock;
    OutfitLock.bOutfit = true;
    FRandomStream OutfitLockedRandom(74019);
    const FDGFullCharacterCustomization OutfitLocked =
        DiscGolfFullCharacterRuntime::Randomize(
            Current, OutfitLock, nullptr, nullptr, OutfitLockedRandom);
    TestTrue(TEXT("A locked outfit prevents an incompatible body randomization"),
        Session7NearlyEqual(OutfitLocked.Body.HeightCm, Current.Body.HeightCm)
        && Session7NearlyEqual(
            OutfitLocked.Body.WingspanScale, Current.Body.WingspanScale)
        && DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            OutfitLocked.Outfit, Current.Outfit));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFullCharacterMissingCosmeticTest,
    "DiscGolfTour.Character.Session7.FullCharacter.MissingCosmeticFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFullCharacterMissingCosmeticTest::RunTest(const FString& Parameters)
{
    FDGFullCharacterCustomization Requested =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    Requested.Identity.VoiceId = TEXT("voice_deleted");
    Requested.Identity.PronounSetId = TEXT("pronouns_deleted");
    Requested.Hair.HairStyleId = TEXT("hair_deleted");
    Requested.Hair.FacialHairId = TEXT("facialhair_deleted");
    Requested.Hair.EyebrowId = TEXT("brow_deleted");
    Requested.Appearance.ScarId = TEXT("scar_deleted");
    Requested.Appearance.TattooIds = {TEXT("tattoo_deleted")};
    Requested.Outfit.Equipped.Add(MakeOutfitEntry(
        EDGOutfitSlot::Top, TEXT("outfit_deleted"), TEXT("Default")));

    const FDiscGolfFullCustomizationResolution Resolution =
        DiscGolfFullCharacterRuntime::ResolveForRuntime(
            Requested, nullptr, nullptr);
    TestFalse(TEXT("Missing catalogs are reported without rejecting the character"),
        Resolution.bAllCosmeticsResolved);
    TestTrue(TEXT("Missing hair, beard and brow use exact stable fallbacks"),
        Resolution.Character.Hair.HairStyleId == FName(TEXT("hair_none"))
        && Resolution.Character.Hair.FacialHairId
            == FName(TEXT("facialhair_none"))
        && Resolution.Character.Hair.EyebrowId == FName(TEXT("brow_default")));
    TestTrue(TEXT("Missing identity options use exact stable fallbacks"),
        Resolution.Character.Identity.VoiceId == FName(TEXT("voice_default"))
        && Resolution.Character.Identity.PronounSetId
            == FName(TEXT("pronouns_default")));
    TestEqual(TEXT("A missing scar safely resolves to none"),
        Resolution.Character.Appearance.ScarId,
        FName(TEXT("scar_none")));
    TestTrue(TEXT("Missing tattoos skip safely and tattoo_none is never persisted"),
        Resolution.Character.Appearance.TattooIds.IsEmpty());
    TestTrue(TEXT("A missing outfit catalog resolves to the safe empty loadout"),
        Resolution.Character.Outfit.Equipped.IsEmpty());
    TestTrue(TEXT("Every missing-content recovery emits diagnostic status"),
        Resolution.Warnings.Num() >= 8);

    UDiscGolfCosmeticCatalog* Catalog = NewObject<UDiscGolfCosmeticCatalog>();
    AddCosmetic(*Catalog, EDGCosmeticKind::Tattoo, TEXT("tattoo_none"));
    AddCosmetic(*Catalog, EDGCosmeticKind::Voice, TEXT("voice_selectable"));
    FString Status;
    FDGFullCharacterCustomization Draft =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    Draft.Appearance.TattooIds = {TEXT("tattoo_old")};
    TestTrue(TEXT("The explicit no-tattoo catalog option is selectable"),
        DiscGolfFullCharacterRuntime::SetCosmeticSelection(
            Draft, EDGCosmeticKind::Tattoo, TEXT("tattoo_none"), Catalog, Status));
    TestTrue(TEXT("Selecting tattoo_none canonicalizes to an empty stable-ID array"),
        Draft.Appearance.TattooIds.IsEmpty());
    TestTrue(TEXT("Identity catalog options are selectable through the shared adapter"),
        DiscGolfFullCharacterRuntime::SetCosmeticSelection(
            Draft, EDGCosmeticKind::Voice, TEXT("voice_selectable"), Catalog, Status));
    TestEqual(TEXT("The selected voice remains a stable ID"),
        Draft.Identity.VoiceId, FName(TEXT("voice_selectable")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFullCharacterNestedSaveGameFlagsTest,
    "DiscGolfTour.Character.Session7.Persistence.NestedSaveGameFlags",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFullCharacterNestedSaveGameFlagsTest::RunTest(
    const FString& Parameters)
{
    const FProperty* FullPayloadProperty = FindFProperty<FProperty>(
        UDiscGolfSaveGame::StaticClass(),
        GET_MEMBER_NAME_CHECKED(UDiscGolfSaveGame, CharacterCustomization));
    TestTrue(TEXT("The schema-10 root full-character payload carries SaveGame"),
        FullPayloadProperty
        && FullPayloadProperty->HasAnyPropertyFlags(CPF_SaveGame));

    const UScriptStruct* Structs[] = {
        FDGFullCharacterCustomization::StaticStruct(),
        FDGIdentityProfile::StaticStruct(),
        FDGBodyProfile::StaticStruct(),
        FDGBodyBuildProfile::StaticStruct(),
        FDGFaceProfile::StaticStruct(),
        FDGHairProfile::StaticStruct(),
        FDGAppearanceProfile::StaticStruct(),
        FDGThrowStyle::StaticStruct(),
        FDGOutfitLoadout::StaticStruct(),
        FDGEquippedOutfitEntry::StaticStruct()
    };
    for (const UScriptStruct* Struct : Structs)
    {
        TestNotNull(TEXT("A full-character nested struct is reflected"), Struct);
        if (!Struct)
        {
            continue;
        }
        for (TFieldIterator<FProperty> It(Struct, EFieldIterationFlags::None);
            It; ++It)
        {
            const FProperty* Property = *It;
            TestTrue(*FString::Printf(
                TEXT("%s.%s carries SaveGame"),
                *Struct->GetName(), *Property->GetName()),
                Property->HasAnyPropertyFlags(CPF_SaveGame));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFullCharacterMemoryRoundTripTest,
    "DiscGolfTour.Character.Session7.Persistence.MemoryRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFullCharacterMemoryRoundTripTest::RunTest(const FString& Parameters)
{
    const FDGFullCharacterCustomization Expected = MakeRoundTripFixture();
    UDiscGolfSaveGame* Source = NewObject<UDiscGolfSaveGame>();
    Source->CharacterCustomization = Expected;

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

    TestEqual(TEXT("The nested payload retains schema 10"),
        Restored->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestTrue(TEXT("Every nested backend/identity/body/face/hair/appearance/throw/outfit field survives SaveGame serialization"),
        DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Expected, Restored->CharacterCustomization));
    TestTrue(TEXT("Serialized full-character power and spin remain presentation-neutral"),
        Session7NearlyEqual(Restored->CharacterCustomization.ThrowStyle.PowerMultiplier, 1.0f)
        && Session7NearlyEqual(Restored->CharacterCustomization.ThrowStyle.SpinMultiplier, 1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFullCharacterSchema8MigrationTest,
    "DiscGolfTour.Character.Session7.Persistence.Schema8To9Migration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFullCharacterSchema8MigrationTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Legacy = NewObject<UDiscGolfSaveGame>();
    Legacy->SaveSchemaVersion = 8;
    Legacy->CharacterProfile.bLeftHanded = true;
    Legacy->CharacterProfile.HeightCm = 201.0f;
    Legacy->CharacterProfile.WingspanScale = 1.06f;
    Legacy->CharacterProfile.Muscularity = 0.72f;
    Legacy->CharacterProfile.BodyFat = 0.18f;
    Legacy->CharacterProfile.ReachBackAmount = 0.87f;
    Legacy->CharacterProfile.FollowThrough = 0.94f;
    Legacy->CharacterProfile.Sanitize();
    Legacy->OutfitLoadout.Equipped.Add(MakeOutfitEntry(
        EDGOutfitSlot::Top, TEXT("proxy_s6_top_tee_01"), TEXT("Teal")));
    Legacy->CharacterCustomization.Identity.DisplayName =
        TEXT("Synthetic pre-schema-9 poison");
    Legacy->CharacterCustomization.AvatarBackendId =
        TEXT("metahuman_assembled");
    Legacy->CharacterCustomization.Hair.HairStyleId = TEXT("hair_poison");

    const DiscGolfProfilePersistence::EMigrationResult Migration =
        DiscGolfProfilePersistence::MigrateToCurrent(*Legacy);
    TestTrue(TEXT("Schema 8 performs the explicit full-character migration"),
        Migration == DiscGolfProfilePersistence::EMigrationResult::Migrated);
    TestEqual(TEXT("Schema 8 advances through schema 10"),
        Legacy->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestTrue(TEXT("Legacy body, build, throw and handedness populate the sole current payload"),
        Session7NearlyEqual(Legacy->CharacterCustomization.Body.HeightCm, 201.0f)
        && Session7NearlyEqual(Legacy->CharacterCustomization.Body.WingspanScale, 1.06f)
        && Session7NearlyEqual(Legacy->CharacterCustomization.BodyBuild.Muscularity, 0.72f)
        && Session7NearlyEqual(Legacy->CharacterCustomization.BodyBuild.BodyFat, 0.18f)
        && Session7NearlyEqual(Legacy->CharacterCustomization.ThrowStyle.ReachBackAmount, 0.87f)
        && Session7NearlyEqual(Legacy->CharacterCustomization.ThrowStyle.FollowThrough, 0.94f)
        && Legacy->CharacterCustomization.Identity.Handedness
            == EDGHandedness::Left);
    TestTrue(TEXT("Legacy outfit stable IDs and variants migrate intact"),
        DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Legacy->CharacterCustomization.Outfit, Legacy->OutfitLoadout));
    const FDGEquippedOutfitEntry* MigratedTop =
        DiscGolfOutfitRuntime::FindEntryForSlot(
            Legacy->CharacterCustomization.Outfit, EDGOutfitSlot::Top);
    TestTrue(TEXT("The schema-8 item and variant identities survive migration"),
        MigratedTop
        && MigratedTop->ItemId == FName(TEXT("proxy_s6_top_tee_01"))
        && MigratedTop->VariantId == FName(TEXT("Teal")));
    TestTrue(TEXT("New fields initialize from deterministic schema defaults"),
        Legacy->CharacterCustomization.AvatarBackendId
            == FName(TEXT("dg_master"))
        && Legacy->CharacterCustomization.Identity.DisplayName == TEXT("Player")
        && Legacy->CharacterCustomization.Hair.HairStyleId
            == FName(TEXT("hair_none"))
        && Legacy->CharacterCustomization.Face.MorphValues.Num() == 20);
    TestTrue(TEXT("Migration cannot introduce a second power or spin authority"),
        Session7NearlyEqual(Legacy->CharacterCustomization.ThrowStyle.PowerMultiplier, 1.0f)
        && Session7NearlyEqual(Legacy->CharacterCustomization.ThrowStyle.SpinMultiplier, 1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFullCharacterCurrentAuthorityTest,
    "DiscGolfTour.Character.Session7.Persistence.Schema9SoleAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFullCharacterCurrentAuthorityTest::RunTest(
    const FString& Parameters)
{
    UDiscGolfSaveGame* Legacy9 = NewObject<UDiscGolfSaveGame>();
    Legacy9->SaveSchemaVersion = 9;
    Legacy9->CharacterCustomization = MakeRoundTripFixture();
    const DiscGolfProfilePersistence::EMigrationResult LegacyMigration =
        DiscGolfProfilePersistence::MigrateToCurrent(*Legacy9);
    TestTrue(TEXT("A schema-9 payload performs the backend migration"),
        LegacyMigration == DiscGolfProfilePersistence::EMigrationResult::Migrated);
    TestEqual(TEXT("Schema 9 advances exactly to schema 10"),
        Legacy9->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestEqual(TEXT("Every pre-schema-10 save deterministically requests DGMaster"),
        Legacy9->CharacterCustomization.AvatarBackendId,
        FName(TEXT("dg_master")));
    TestTrue(TEXT("The schema-9 backend migration is idempotent"),
        DiscGolfProfilePersistence::MigrateToCurrent(*Legacy9)
            == DiscGolfProfilePersistence::EMigrationResult::AlreadyCurrent);
    TestTrue(TEXT("The backend migration preserves existing full-character truth"),
        Session7NearlyEqual(
            Legacy9->CharacterCustomization.Body.HeightCm, 201.0f)
        && Legacy9->CharacterCustomization.Identity.Handedness
            == EDGHandedness::Left);

    UDiscGolfSaveGame* Current = NewObject<UDiscGolfSaveGame>();
    Current->SaveSchemaVersion = DiscGolfSaveSchema::CurrentVersion;
    Current->CharacterCustomization = MakeRoundTripFixture();
    Current->CharacterCustomization.ThrowStyle.PowerMultiplier = 1.25f;
    Current->CharacterCustomization.ThrowStyle.SpinMultiplier = 0.75f;
    Current->CharacterProfile.HeightCm = 151.0f;
    Current->CharacterProfile.bLeftHanded = false;
    Current->OutfitLoadout.Equipped = {MakeOutfitEntry(
        EDGOutfitSlot::Bottom, TEXT("legacy_mirror_must_not_win"), TEXT("Default"))};

    const DiscGolfProfilePersistence::EMigrationResult Migration =
        DiscGolfProfilePersistence::MigrateToCurrent(*Current);
    TestTrue(TEXT("A schema-10 payload needs no version migration"),
        Migration == DiscGolfProfilePersistence::EMigrationResult::AlreadyCurrent);
    TestTrue(TEXT("Schema-10 reads the complete payload instead of legacy mirrors"),
        Session7NearlyEqual(Current->CharacterCustomization.Body.HeightCm, 201.0f)
        && Current->CharacterCustomization.Identity.Handedness
            == EDGHandedness::Left
        && Current->CharacterCustomization.Outfit.Equipped.Num() == 2);
    TestTrue(TEXT("Current schema normalization preserves gameplay power/spin authority"),
        Session7NearlyEqual(
            Current->CharacterCustomization.ThrowStyle.PowerMultiplier, 1.0f)
        && Session7NearlyEqual(
            Current->CharacterCustomization.ThrowStyle.SpinMultiplier, 1.0f));
    TestEqual(TEXT("Current requested backend persists without content resolution"),
        Current->CharacterCustomization.AvatarBackendId,
        FName(TEXT("metahuman_assembled")));
    TestTrue(TEXT("Schema-10 rewrites the Session 4 mirror from full truth"),
        Session7NearlyEqual(Current->CharacterProfile.HeightCm, 201.0f)
        && Current->CharacterProfile.GetHandedness() == EDGHandedness::Left);
    TestTrue(TEXT("Schema-10 rewrites the Session 6 mirror from full truth"),
        DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            Current->OutfitLoadout, Current->CharacterCustomization.Outfit));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFullCharacterDiskRoundTripTest,
    "DiscGolfTour.Character.Session7.Persistence.DiskRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFullCharacterDiskRoundTripTest::RunTest(const FString& Parameters)
{
    const FString SlotName = FString::Printf(
        TEXT("DiscGolfTour_Automation_Session7FullCharacter_Unit_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FScopedFullCharacterSaveSlotCleanup Cleanup(SlotName);
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);

    const FDGFullCharacterCustomization Expected = MakeRoundTripFixture();
    UDiscGolfSaveGame* Source = NewObject<UDiscGolfSaveGame>();
    Source->CharacterCustomization = Expected;
    const bool bSaved = UGameplayStatics::SaveGameToSlot(Source, SlotName, 0);
    TestTrue(TEXT("The complete schema-10 character saves to an isolated disk slot"),
        bSaved);

    UDiscGolfSaveGame* Restored = bSaved
        ? Cast<UDiscGolfSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0))
        : nullptr;
    TestNotNull(TEXT("The isolated schema-10 disk slot reloads"), Restored);
    if (Restored)
    {
        TestEqual(TEXT("The disk payload retains schema 10"),
            Restored->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
        TestTrue(TEXT("The complete character reconstructs from the real disk path"),
            DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
                Expected, Restored->CharacterCustomization));
    }

    const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    TestTrue(TEXT("The isolated Session 7 disk slot is deleted"), bDeleted);
    TestFalse(TEXT("Session 7 disk cleanup is verified"),
        UGameplayStatics::DoesSaveGameExist(SlotName, 0));
    return true;
}

#endif
