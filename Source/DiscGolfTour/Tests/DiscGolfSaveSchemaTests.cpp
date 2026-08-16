#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfSaveGame.h"
#include "../DiscGolfTourGameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfDefaultSaveSchemaVersionTest,
    "DiscGolfTour.Persistence.SaveSchema.DefaultVersion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfDefaultSaveSchemaVersionTest::RunTest(const FString& Parameters)
{
    const UDiscGolfSaveGame* Save = NewObject<UDiscGolfSaveGame>();
    TestNotNull(TEXT("A default save object can be created"), Save);
    if (!Save)
    {
        return false;
    }

    TestEqual(TEXT("A default save uses the authoritative current schema"),
        Save->SaveSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestTrue(TEXT("A default save is accepted by the exact-version gate"),
        DiscGolfSaveSchema::IsCurrent(Save->SaveSchemaVersion));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCurrentSaveSchemaAcceptanceTest,
    "DiscGolfTour.Persistence.SaveSchema.CurrentVersionAccepted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCurrentSaveSchemaAcceptanceTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("The current migration target is Session 6 schema 8"),
        DiscGolfSaveSchema::CurrentVersion, 8);
    TestTrue(TEXT("The exact current schema is accepted"),
        DiscGolfSaveSchema::IsCurrent(DiscGolfSaveSchema::CurrentVersion));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfNonCurrentSaveSchemaRejectionTest,
    "DiscGolfTour.Persistence.SaveSchema.StaleAndFutureVersionsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfNonCurrentSaveSchemaRejectionTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("A stale, unmigrated schema is rejected"),
        DiscGolfSaveSchema::IsCurrent(DiscGolfSaveSchema::CurrentVersion - 1));
    TestFalse(TEXT("An unknown future schema is rejected"),
        DiscGolfSaveSchema::IsCurrent(DiscGolfSaveSchema::CurrentVersion + 1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession6ValidationSaveSlotGuardTest,
    "DiscGolfTour.Persistence.SaveSchema.Session6ValidationSlotGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession6ValidationSaveSlotGuardTest::RunTest(const FString& Parameters)
{
    const FString SafeSlot =
        TEXT("DiscGolfTour_Automation_Session6Outfit_ABC_123");
    FString Resolved = TEXT("caller_value_must_be_cleared");
    TestTrue(TEXT("A safe slot resolves only behind both Session 6 validation guards"),
        DiscGolfProfilePersistence::TryResolveSession6OutfitValidationSaveSlot(
            TEXT("-Session6OutfitVisualCapture -Session6OutfitValidationNoSave -Session6OutfitValidationSaveSlot=DiscGolfTour_Automation_Session6Outfit_ABC_123"),
            Resolved));
    TestEqual(TEXT("The exact safe validation slot is retained"), Resolved, SafeSlot);

    const auto TestRejected = [this](const TCHAR* Label, const TCHAR* CommandLine)
    {
        FString Rejected = TEXT("caller_value_must_be_cleared");
        TestFalse(Label,
            DiscGolfProfilePersistence::TryResolveSession6OutfitValidationSaveSlot(
                CommandLine, Rejected));
        TestTrue(TEXT("Rejected validation slots clear caller output"), Rejected.IsEmpty());
    };
    TestRejected(
        TEXT("The visual-capture guard is mandatory"),
        TEXT("-Session6OutfitValidationNoSave -Session6OutfitValidationSaveSlot=DiscGolfTour_Automation_Session6Outfit_ABC"));
    TestRejected(
        TEXT("The explicit no-save guard is mandatory"),
        TEXT("-Session6OutfitVisualCapture -Session6OutfitValidationSaveSlot=DiscGolfTour_Automation_Session6Outfit_ABC"));
    TestRejected(
        TEXT("The automation prefix is mandatory"),
        TEXT("-Session6OutfitVisualCapture -Session6OutfitValidationNoSave -Session6OutfitValidationSaveSlot=DiscGolfTour_Profile_0"));
    TestRejected(
        TEXT("The validation suffix cannot be empty"),
        TEXT("-Session6OutfitVisualCapture -Session6OutfitValidationNoSave -Session6OutfitValidationSaveSlot=DiscGolfTour_Automation_Session6Outfit_"));
    TestRejected(
        TEXT("Path punctuation is rejected"),
        TEXT("-Session6OutfitVisualCapture -Session6OutfitValidationNoSave -Session6OutfitValidationSaveSlot=DiscGolfTour_Automation_Session6Outfit_../Profile"));
    TestRejected(
        TEXT("The validation suffix has a strict length bound"),
        TEXT("-Session6OutfitVisualCapture -Session6OutfitValidationNoSave -Session6OutfitValidationSaveSlot=DiscGolfTour_Automation_Session6Outfit_1234567890123456789012345678901234567890123456789"));
    return true;
}

#endif
