#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfTourGameInstance.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGameInstancePersistencePolicyTest,
    "DiscGolfTour.Persistence.ProcessPolicy.EphemeralDeveloperRuns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGameInstancePersistencePolicyTest::RunTest(const FString& Parameters)
{
    const auto TestPolicy = [this](
        const TCHAR* Label,
        const TCHAR* CommandLine,
        bool bExpectedLoad,
        bool bExpectedWrites)
    {
        const DiscGolfProfilePersistence::FProcessPolicy Policy =
            DiscGolfProfilePersistence::ResolveProcessPolicy(CommandLine);
        TestEqual(FString::Printf(TEXT("%s load policy"), Label),
            Policy.bLoadExistingProfile, bExpectedLoad);
        TestEqual(FString::Printf(TEXT("%s write policy"), Label),
            Policy.bAllowProfileWrites, bExpectedWrites);
        TestEqual(FString::Printf(TEXT("%s in-memory commit policy"), Label),
            Policy.AcceptsProfileWritesInMemoryOnly(), !bExpectedWrites);
    };

    TestPolicy(TEXT("Null command line"), nullptr, true, true);
    TestPolicy(TEXT("Ordinary launch"), TEXT("-unattended -NullRHI"), true, true);
    TestPolicy(TEXT("No-load launch"), TEXT("-NoLoadExistingSave"), false, false);
    TestPolicy(TEXT("Read-only profile launch"), TEXT("-DGNoProfileWrites"), true, false);
    TestPolicy(TEXT("Both ephemeral guards"),
        TEXT("-NoLoadExistingSave -DGNoProfileWrites"), false, false);
    TestPolicy(TEXT("Legacy practice-snapshot guard"),
        TEXT("-DGDeveloperToolNoSave"), true, true);
    TestPolicy(TEXT("Unrelated no-save spelling"), TEXT("-NoSave"), true, true);

    const DiscGolfProfilePersistence::FProcessPolicy MalformedReleaseCapture =
        DiscGolfProfilePersistence::ResolveProcessPolicy(
            TEXT("-performancecaptureTypo -Hole=2"), true);
    TestFalse(TEXT("malformed Shipping capture attempt cannot load the profile"),
        MalformedReleaseCapture.bLoadExistingProfile);
    TestFalse(TEXT("malformed Shipping capture attempt cannot write the profile"),
        MalformedReleaseCapture.bAllowProfileWrites);

    const DiscGolfProfilePersistence::FProcessPolicy DevelopmentCapture =
        DiscGolfProfilePersistence::ResolveProcessPolicy(
            TEXT("-PerformanceCaptureSeconds=30"), false);
    TestTrue(TEXT("Development capture keeps the historical load policy"),
        DevelopmentCapture.bLoadExistingProfile);
    TestTrue(TEXT("Development capture keeps the historical write policy"),
        DevelopmentCapture.bAllowProfileWrites);
    return true;
}

#endif
