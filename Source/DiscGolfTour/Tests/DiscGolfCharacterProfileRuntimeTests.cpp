#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "../DiscGolfCharacterProfileRuntime.h"
#include "../DiscGolfOutfitRuntime.h"
#include "../DiscGolfSaveGame.h"
#include "../DiscGolfTourGameInstance.h"

#include <limits>

namespace
{
    bool NearlyEqual(float A, float B)
    {
        return FMath::IsNearlyEqual(A, B, 1.0e-5f);
    }

    class FScopedAutomationSaveSlotCleanup
    {
    public:
        FScopedAutomationSaveSlotCleanup(FString InSlotName, const int32 InUserIndex)
            : SlotName(MoveTemp(InSlotName))
            , UserIndex(InUserIndex)
        {
        }

        ~FScopedAutomationSaveSlotCleanup()
        {
            // Best-effort cleanup also covers an assertion failure or an added early return.
            UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
        }

    private:
        FString SlotName;
        int32 UserIndex = 0;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCharacterProfileFrameworkRoundTripTest,
    "DiscGolfTour.Character.Session4.Profile.FrameworkRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCharacterProfileFrameworkRoundTripTest::RunTest(const FString& Parameters)
{
    FDGBodyProfile Body;
    Body.HeightCm = 198.0f;
    Body.WingspanScale = 1.06f;
    Body.ShoulderWidthScale = 1.04f;
    Body.TorsoLengthScale = 1.03f;
    Body.LegLengthScale = 1.05f;
    Body.HandScale = 1.02f;
    Body.MassKg = 96.0f;

    FDGBodyBuildProfile Build;
    Build.Muscularity = 0.72f;
    Build.BodyFat = 0.18f;
    Build.Chest = 0.4f;
    Build.Waist = -0.2f;
    Build.Hips = 0.1f;
    Build.Arms = 0.3f;
    Build.Legs = 0.25f;

    FDGThrowStyle Style;
    Style.RunUpIntensity = 0.82f;
    Style.ReachBackAmount = 0.91f;
    Style.TorsoRotation = 0.69f;
    Style.BraceIntensity = 0.88f;
    Style.Explosiveness = 0.77f;
    Style.FollowThrough = 0.86f;
    Style.PowerMultiplier = 1.24f;
    Style.SpinMultiplier = 0.76f;

    const FDiscGolfCharacterProfileSaveData Saved =
        FDiscGolfCharacterProfileSaveData::FromFramework(
            Body, Style, EDGHandedness::Left, Build);
    const FDGBodyProfile RestoredBody = Saved.ToBodyProfile();
    const FDGBodyBuildProfile RestoredBuild = Saved.ToBodyBuildProfile();
    const FDGThrowStyle RestoredStyle = Saved.ToThrowStyle();

    TestTrue(TEXT("Handedness survives the primitive DTO boundary"),
        Saved.GetHandedness() == EDGHandedness::Left);
    TestTrue(TEXT("Height survives framework conversion"),
        NearlyEqual(RestoredBody.HeightCm, Body.HeightCm));
    TestTrue(TEXT("Wingspan survives framework conversion"),
        NearlyEqual(RestoredBody.WingspanScale, Body.WingspanScale));
    TestTrue(TEXT("Shoulder width survives framework conversion"),
        NearlyEqual(RestoredBody.ShoulderWidthScale, Body.ShoulderWidthScale));
    TestTrue(TEXT("Torso length survives framework conversion"),
        NearlyEqual(RestoredBody.TorsoLengthScale, Body.TorsoLengthScale));
    TestTrue(TEXT("Leg length survives framework conversion"),
        NearlyEqual(RestoredBody.LegLengthScale, Body.LegLengthScale));
    TestTrue(TEXT("Hand scale survives framework conversion"),
        NearlyEqual(RestoredBody.HandScale, Body.HandScale));
    TestTrue(TEXT("Mass survives framework conversion"),
        NearlyEqual(RestoredBody.MassKg, Body.MassKg));
    TestTrue(TEXT("Body-build foundation survives framework conversion"),
        NearlyEqual(RestoredBuild.Muscularity, Build.Muscularity)
        && NearlyEqual(RestoredBuild.BodyFat, Build.BodyFat)
        && NearlyEqual(RestoredBuild.Chest, Build.Chest)
        && NearlyEqual(RestoredBuild.Waist, Build.Waist)
        && NearlyEqual(RestoredBuild.Hips, Build.Hips)
        && NearlyEqual(RestoredBuild.Arms, Build.Arms)
        && NearlyEqual(RestoredBuild.Legs, Build.Legs));
    TestTrue(TEXT("Six visual style values survive framework conversion"),
        NearlyEqual(RestoredStyle.RunUpIntensity, Style.RunUpIntensity)
        && NearlyEqual(RestoredStyle.ReachBackAmount, Style.ReachBackAmount)
        && NearlyEqual(RestoredStyle.TorsoRotation, Style.TorsoRotation)
        && NearlyEqual(RestoredStyle.BraceIntensity, Style.BraceIntensity)
        && NearlyEqual(RestoredStyle.Explosiveness, Style.Explosiveness)
        && NearlyEqual(RestoredStyle.FollowThrough, Style.FollowThrough));
    TestTrue(TEXT("Creator persistence cannot alter the power multiplier"),
        NearlyEqual(RestoredStyle.PowerMultiplier, 1.0f));
    TestTrue(TEXT("Creator persistence cannot alter the spin multiplier"),
        NearlyEqual(RestoredStyle.SpinMultiplier, 1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCharacterProfileSanitizationTest,
    "DiscGolfTour.Character.Session4.Profile.Sanitization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCharacterProfileSanitizationTest::RunTest(const FString& Parameters)
{
    FDiscGolfCharacterProfileSaveData Data;
    Data.HeightCm = std::numeric_limits<float>::quiet_NaN();
    Data.WingspanScale = std::numeric_limits<float>::infinity();
    Data.ShoulderWidthScale = 20.0f;
    Data.TorsoLengthScale = -20.0f;
    Data.LegLengthScale = 20.0f;
    Data.HandScale = -20.0f;
    Data.MassKg = -10.0f;
    Data.Muscularity = std::numeric_limits<float>::quiet_NaN();
    Data.BodyFat = 4.0f;
    Data.Chest = -4.0f;
    Data.RunUpIntensity = std::numeric_limits<float>::infinity();
    Data.ReachBackAmount = -1.0f;
    Data.TorsoRotation = 2.0f;
    Data.BraceIntensity = std::numeric_limits<float>::quiet_NaN();
    Data.Explosiveness = -2.0f;
    Data.FollowThrough = 3.0f;
    Data.Sanitize();

    using namespace DiscGolfCharacterCreatorSchema;
    TestTrue(TEXT("Non-finite height returns to schema default"),
        NearlyEqual(Data.HeightCm, DefaultHeightCm));
    TestTrue(TEXT("Non-finite wingspan returns to schema default"),
        NearlyEqual(Data.WingspanScale, DefaultBodyScale));
    TestTrue(TEXT("Shoulder width clamps to schema maximum"),
        NearlyEqual(Data.ShoulderWidthScale, MaxShoulderWidthScale));
    TestTrue(TEXT("Torso length clamps to schema minimum"),
        NearlyEqual(Data.TorsoLengthScale, MinTorsoLengthScale));
    TestTrue(TEXT("Leg length clamps to schema maximum"),
        NearlyEqual(Data.LegLengthScale, MaxLegLengthScale));
    TestTrue(TEXT("Hand scale clamps to schema minimum"),
        NearlyEqual(Data.HandScale, MinHandScale));
    TestTrue(TEXT("Mass clamps to schema minimum"), NearlyEqual(Data.MassKg, MinMassKg));
    TestTrue(TEXT("Non-finite muscularity returns to foundation default"),
        NearlyEqual(Data.Muscularity, DefaultMuscularity));
    TestTrue(TEXT("Body fat and shape values clamp"),
        NearlyEqual(Data.BodyFat, 1.0f) && NearlyEqual(Data.Chest, -1.0f));
    TestTrue(TEXT("Non-finite run-up returns to style default"),
        NearlyEqual(Data.RunUpIntensity, DefaultRunUpIntensity));
    TestTrue(TEXT("Reachback clamps to zero"), NearlyEqual(Data.ReachBackAmount, 0.0f));
    TestTrue(TEXT("Torso rotation clamps to one"), NearlyEqual(Data.TorsoRotation, 1.0f));
    TestTrue(TEXT("Non-finite brace returns to style default"),
        NearlyEqual(Data.BraceIntensity, DefaultBraceIntensity));
    TestTrue(TEXT("Remaining style values clamp"),
        NearlyEqual(Data.Explosiveness, 0.0f) && NearlyEqual(Data.FollowThrough, 1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCharacterProfileSaveGameRoundTripTest,
    "DiscGolfTour.Character.Session4.Profile.SaveGameRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCharacterProfileSaveGameRoundTripTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Source = NewObject<UDiscGolfSaveGame>();
    Source->CharacterProfile.bLeftHanded = true;
    Source->CharacterProfile.HeightCm = 205.0f;
    Source->CharacterProfile.WingspanScale = 1.07f;
    Source->CharacterProfile.MassKg = 101.0f;
    Source->CharacterProfile.Muscularity = 0.81f;
    Source->CharacterProfile.BodyFat = 0.16f;
    Source->CharacterProfile.ReachBackAmount = 0.93f;
    Source->CharacterProfile.FollowThrough = 0.89f;

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

    TestEqual(TEXT("Save schema survives binary serialization"),
        Restored->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestTrue(TEXT("Primitive handedness survives binary serialization"),
        Restored->CharacterProfile.bLeftHanded);
    TestTrue(TEXT("Body values survive binary serialization"),
        NearlyEqual(Restored->CharacterProfile.HeightCm, 205.0f)
        && NearlyEqual(Restored->CharacterProfile.WingspanScale, 1.07f)
        && NearlyEqual(Restored->CharacterProfile.MassKg, 101.0f));
    TestTrue(TEXT("Body-build values survive binary serialization"),
        NearlyEqual(Restored->CharacterProfile.Muscularity, 0.81f)
        && NearlyEqual(Restored->CharacterProfile.BodyFat, 0.16f));
    TestTrue(TEXT("Throw-style values survive binary serialization"),
        NearlyEqual(Restored->CharacterProfile.ReachBackAmount, 0.93f)
        && NearlyEqual(Restored->CharacterProfile.FollowThrough, 0.89f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCharacterProfileDiskSlotRoundTripTest,
    "DiscGolfTour.Character.Session4.Profile.DiskSlotRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCharacterProfileDiskSlotRoundTripTest::RunTest(const FString& Parameters)
{
    constexpr int32 UserIndex = 0;
    const FString SlotName = FString::Printf(
        TEXT("DiscGolfTour_Automation_Session4Profile_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FScopedAutomationSaveSlotCleanup Cleanup(SlotName, UserIndex);

    // The GUID makes collision effectively impossible, but preserve the invariant that the
    // automation-only slot starts empty before proving the real platform SaveGame path.
    UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
    TestFalse(TEXT("Automation-only disk slot starts absent"),
        UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex));

    UDiscGolfSaveGame* Source = NewObject<UDiscGolfSaveGame>();
    TestNotNull(TEXT("Session 4 disk-slot fixture is created"), Source);

    bool bSaved = false;
    FDiscGolfCharacterProfileSaveData Expected;
    FDGOutfitLoadout ExpectedOutfit;
    if (Source)
    {
        Source->SaveSchemaVersion = DiscGolfSaveSchema::CurrentVersion;
        Source->CharacterProfile.bLeftHanded = true;
        Source->CharacterProfile.HeightCm = 201.0f;
        Source->CharacterProfile.WingspanScale = 1.06f;
        Source->CharacterProfile.ShoulderWidthScale = 0.95f;
        Source->CharacterProfile.TorsoLengthScale = 1.04f;
        Source->CharacterProfile.LegLengthScale = 0.96f;
        Source->CharacterProfile.HandScale = 1.05f;
        Source->CharacterProfile.MassKg = 97.0f;
        Source->CharacterProfile.Muscularity = 0.72f;
        Source->CharacterProfile.BodyFat = 0.18f;
        Source->CharacterProfile.Chest = 0.40f;
        Source->CharacterProfile.Waist = -0.30f;
        Source->CharacterProfile.Hips = 0.20f;
        Source->CharacterProfile.Arms = 0.55f;
        Source->CharacterProfile.Legs = -0.45f;
        Source->CharacterProfile.RunUpIntensity = 0.91f;
        Source->CharacterProfile.ReachBackAmount = 0.87f;
        Source->CharacterProfile.TorsoRotation = 0.62f;
        Source->CharacterProfile.BraceIntensity = 0.78f;
        Source->CharacterProfile.Explosiveness = 0.69f;
        Source->CharacterProfile.FollowThrough = 0.94f;
        Source->CharacterProfile.Sanitize();
        Expected = Source->CharacterProfile;

        FDGEquippedOutfitEntry Top;
        Top.Slot = EDGOutfitSlot::Top;
        Top.ItemId = TEXT("proxy_s6_top_tee_01");
        Top.VariantId = TEXT("Teal");
        Source->OutfitLoadout.Equipped.Add(Top);
        FDGEquippedOutfitEntry Bag;
        Bag.Slot = EDGOutfitSlot::Bag;
        Bag.ItemId = TEXT("proxy_s6_bag_backpack_01");
        Bag.VariantId = TEXT("Graphite");
        Source->OutfitLoadout.Equipped.Add(Bag);
        ExpectedOutfit = DiscGolfOutfitRuntime::NormalizeForPersistence(
            Source->OutfitLoadout);

        bSaved = UGameplayStatics::SaveGameToSlot(Source, SlotName, UserIndex);
    }

    TestTrue(TEXT("Sanitized Session 4 profile saves through the real disk-slot API"), bSaved);
    TestTrue(TEXT("Saved automation-only disk slot exists"),
        UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex));

    USaveGame* LoadedObject = bSaved
        ? UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex)
        : nullptr;
    UDiscGolfSaveGame* Restored = Cast<UDiscGolfSaveGame>(LoadedObject);
    TestNotNull(TEXT("Disk-slot payload reloads as UDiscGolfSaveGame"), Restored);

    if (Restored)
    {
        const FDiscGolfCharacterProfileSaveData& Actual = Restored->CharacterProfile;
        TestEqual(TEXT("Disk-slot payload retains schema 10"),
            Restored->SaveSchemaVersion, 10);
        TestEqual(TEXT("Disk-slot payload retains the current save schema"),
            Restored->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
        TestTrue(TEXT("Handedness survives disk-slot persistence"),
            Actual.bLeftHanded == Expected.bLeftHanded
            && Actual.GetHandedness() == EDGHandedness::Left);
        TestTrue(TEXT("Height survives disk-slot persistence"),
            NearlyEqual(Actual.HeightCm, Expected.HeightCm));
        TestTrue(TEXT("Wingspan survives disk-slot persistence"),
            NearlyEqual(Actual.WingspanScale, Expected.WingspanScale));
        TestTrue(TEXT("Shoulder width survives disk-slot persistence"),
            NearlyEqual(Actual.ShoulderWidthScale, Expected.ShoulderWidthScale));
        TestTrue(TEXT("Torso length survives disk-slot persistence"),
            NearlyEqual(Actual.TorsoLengthScale, Expected.TorsoLengthScale));
        TestTrue(TEXT("Leg length survives disk-slot persistence"),
            NearlyEqual(Actual.LegLengthScale, Expected.LegLengthScale));
        TestTrue(TEXT("Hand scale survives disk-slot persistence"),
            NearlyEqual(Actual.HandScale, Expected.HandScale));
        TestTrue(TEXT("Mass survives disk-slot persistence"),
            NearlyEqual(Actual.MassKg, Expected.MassKg));
        TestTrue(TEXT("Muscularity survives disk-slot persistence"),
            NearlyEqual(Actual.Muscularity, Expected.Muscularity));
        TestTrue(TEXT("Body fat survives disk-slot persistence"),
            NearlyEqual(Actual.BodyFat, Expected.BodyFat));
        TestTrue(TEXT("Chest shape survives disk-slot persistence"),
            NearlyEqual(Actual.Chest, Expected.Chest));
        TestTrue(TEXT("Waist shape survives disk-slot persistence"),
            NearlyEqual(Actual.Waist, Expected.Waist));
        TestTrue(TEXT("Hip shape survives disk-slot persistence"),
            NearlyEqual(Actual.Hips, Expected.Hips));
        TestTrue(TEXT("Arm shape survives disk-slot persistence"),
            NearlyEqual(Actual.Arms, Expected.Arms));
        TestTrue(TEXT("Leg shape survives disk-slot persistence"),
            NearlyEqual(Actual.Legs, Expected.Legs));
        TestTrue(TEXT("Run-up intensity survives disk-slot persistence"),
            NearlyEqual(Actual.RunUpIntensity, Expected.RunUpIntensity));
        TestTrue(TEXT("Reachback amount survives disk-slot persistence"),
            NearlyEqual(Actual.ReachBackAmount, Expected.ReachBackAmount));
        TestTrue(TEXT("Torso rotation survives disk-slot persistence"),
            NearlyEqual(Actual.TorsoRotation, Expected.TorsoRotation));
        TestTrue(TEXT("Brace intensity survives disk-slot persistence"),
            NearlyEqual(Actual.BraceIntensity, Expected.BraceIntensity));
        TestTrue(TEXT("Explosiveness survives disk-slot persistence"),
            NearlyEqual(Actual.Explosiveness, Expected.Explosiveness));
        TestTrue(TEXT("Follow-through survives disk-slot persistence"),
            NearlyEqual(Actual.FollowThrough, Expected.FollowThrough));
        TestTrue(TEXT("Stable outfit IDs and variants survive disk-slot persistence"),
            DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
                Restored->OutfitLoadout, ExpectedOutfit));
    }

    const bool bDeleted = UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
    TestTrue(TEXT("Automation-only disk slot is deleted after verification"), bDeleted);
    TestFalse(TEXT("Automation-only disk slot deletion is verified"),
        UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCharacterProfileSchemaMigrationTest,
    "DiscGolfTour.Character.Session4.Profile.Schema6Migration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCharacterProfileSchemaMigrationTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Legacy = NewObject<UDiscGolfSaveGame>();
    Legacy->SaveSchemaVersion = 6;

    const DiscGolfProfilePersistence::EMigrationResult Result =
        DiscGolfProfilePersistence::MigrateToCurrent(*Legacy);

    TestTrue(TEXT("Schema 6 profile performs every migration through Session 8"),
        Result == DiscGolfProfilePersistence::EMigrationResult::Migrated);
    TestEqual(TEXT("Schema 6 profile advances to schema 10"),
        Legacy->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestEqual(TEXT("Pre-schema-10 profiles deterministically request DGMaster"),
        Legacy->CharacterCustomization.AvatarBackendId,
        FName(TEXT("dg_master")));
    TestTrue(TEXT("Legacy save reconstructs baseline body defaults"),
        NearlyEqual(Legacy->CharacterProfile.HeightCm, 183.0f)
        && NearlyEqual(Legacy->CharacterProfile.WingspanScale, 1.0f)
        && NearlyEqual(Legacy->CharacterProfile.MassKg, 82.0f));
    TestTrue(TEXT("Legacy save reconstructs body-build defaults"),
        NearlyEqual(Legacy->CharacterProfile.Muscularity, 0.35f)
        && NearlyEqual(Legacy->CharacterProfile.BodyFat, 0.35f));
    TestTrue(TEXT("Legacy save reconstructs right handedness"),
        Legacy->CharacterProfile.GetHandedness() == EDGHandedness::Right);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCharacterProfileSchema7OutfitMigrationTest,
    "DiscGolfTour.Character.Session6.Outfit.Schema7Migration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCharacterProfileSchema7OutfitMigrationTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Legacy = NewObject<UDiscGolfSaveGame>();
    Legacy->SaveSchemaVersion = 7;

    // A real schema-7 archive has no outfit property. Populate it here to prove
    // that a partially reconstructed/synthetic legacy object cannot smuggle
    // non-schema data into the current profile.
    FDGEquippedOutfitEntry SyntheticEntry;
    SyntheticEntry.Slot = EDGOutfitSlot::Top;
    SyntheticEntry.ItemId = TEXT("not_present_in_schema_7");
    SyntheticEntry.VariantId = TEXT("Default");
    Legacy->OutfitLoadout.Equipped.Add(SyntheticEntry);

    const DiscGolfProfilePersistence::EMigrationResult Result =
        DiscGolfProfilePersistence::MigrateToCurrent(*Legacy);

    TestTrue(TEXT("Schema 7 performs the Session 6 outfit migration"),
        Result == DiscGolfProfilePersistence::EMigrationResult::Migrated);
    TestEqual(TEXT("Schema 7 advances to schema 10"),
        Legacy->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestEqual(TEXT("Schema 7 receives the deterministic DGMaster backend"),
        Legacy->CharacterCustomization.AvatarBackendId,
        FName(TEXT("dg_master")));
    TestTrue(TEXT("Schema 7 deterministically starts with an empty outfit"),
        Legacy->OutfitLoadout.Equipped.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCharacterProfileFutureSchemaGuardTest,
    "DiscGolfTour.Character.Session4.Profile.FutureSchemaGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCharacterProfileFutureSchemaGuardTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Future = NewObject<UDiscGolfSaveGame>();
    Future->SaveSchemaVersion = DiscGolfSaveSchema::CurrentVersion + 1;
    Future->CharacterProfile.HeightCm = 999.0f;
    Future->CharacterCustomization.Identity.DisplayName = TEXT("Future Payload");
    Future->PlayerSettings.GraphicsQuality = 99;

    const DiscGolfProfilePersistence::EMigrationResult Result =
        DiscGolfProfilePersistence::MigrateToCurrent(*Future);

    TestTrue(TEXT("Unknown future schema is explicitly rejected"),
        Result == DiscGolfProfilePersistence::EMigrationResult::FutureSchemaRejected);
    TestEqual(TEXT("Future version remains untouched"),
        Future->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion + 1);
    TestTrue(TEXT("Future character data is not normalized by older code"),
        NearlyEqual(Future->CharacterProfile.HeightCm, 999.0f));
    TestEqual(TEXT("Future full-character data is not normalized by older code"),
        Future->CharacterCustomization.Identity.DisplayName,
        FString(TEXT("Future Payload")));
    TestEqual(TEXT("Future settings data is not normalized by older code"),
        Future->PlayerSettings.GraphicsQuality, 99);

    UDiscGolfTourGameInstance* EmptyInstance = NewObject<UDiscGolfTourGameInstance>();
    FDGBodyProfile CallerFallbackBody;
    CallerFallbackBody.HeightCm = 199.0f;
    FDGThrowStyle CallerFallbackStyle;
    CallerFallbackStyle.ReachBackAmount = 0.91f;
    EDGHandedness CallerFallbackHandedness = EDGHandedness::Left;
    TestFalse(TEXT("Missing/current-incompatible profile is reported to the caller"),
        EmptyInstance->GetCharacterProfile(
            CallerFallbackBody, CallerFallbackStyle, CallerFallbackHandedness));
    TestTrue(TEXT("Failed profile reads preserve caller-owned runtime fallbacks"),
        NearlyEqual(CallerFallbackBody.HeightCm, 199.0f)
        && NearlyEqual(CallerFallbackStyle.ReachBackAmount, 0.91f)
        && CallerFallbackHandedness == EDGHandedness::Left);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCharacterProfileLegacyMigrationRegressionTest,
    "DiscGolfTour.Character.Session4.Profile.LegacyMigrationRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCharacterProfileLegacyMigrationRegressionTest::RunTest(const FString& Parameters)
{
    UDiscGolfSaveGame* Legacy = NewObject<UDiscGolfSaveGame>();
    Legacy->SaveSchemaVersion = 3;
    Legacy->PracticeCourseId = NAME_None;
    Legacy->PracticeLayoutId = NAME_None;
    Legacy->PracticeHoleNumber = 0;
    Legacy->PracticeMoldId = NAME_None;
    Legacy->PreferredGraphicsPreset = 3;

    const DiscGolfProfilePersistence::EMigrationResult Result =
        DiscGolfProfilePersistence::MigrateToCurrent(*Legacy);

    TestTrue(TEXT("Pre-course-schema profile migrates through every historical step"),
        Result == DiscGolfProfilePersistence::EMigrationResult::Migrated);
    TestEqual(TEXT("Legacy profile reaches current schema"),
        Legacy->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestEqual(TEXT("Course identity migration is preserved"),
        Legacy->PracticeCourseId, FName(TEXT("RegressionCourse")));
    TestEqual(TEXT("Layout identity migration is preserved"),
        Legacy->PracticeLayoutId, FName(TEXT("Practice")));
    TestEqual(TEXT("Hole-number migration is preserved"), Legacy->PracticeHoleNumber, 1);
    TestEqual(TEXT("Disc-selection migration is preserved"),
        Legacy->PracticeMoldId, FName(TEXT("Apex")));
    TestEqual(TEXT("Settings migration is preserved"),
        Legacy->PlayerSettings.GraphicsQuality, 3);
    TestTrue(TEXT("Character defaults are introduced after historical migrations"),
        NearlyEqual(Legacy->CharacterProfile.HeightCm, 183.0f)
        && Legacy->CharacterProfile.GetHandedness() == EDGHandedness::Right);
    return true;
}

#endif
