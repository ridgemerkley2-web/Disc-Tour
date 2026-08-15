#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfSaveGame.h"

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
    TestEqual(TEXT("The current migration target remains schema 6"),
        DiscGolfSaveSchema::CurrentVersion, 6);
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

#endif
