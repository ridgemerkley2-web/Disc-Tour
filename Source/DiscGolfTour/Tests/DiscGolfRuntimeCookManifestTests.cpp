#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"
#include "../DiscGolfRuntimeCookManifest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession8BRuntimeCookManifestContractTest,
    "DiscGolfTour.Character.Session8B.CookManifest.RuntimeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession8BRuntimeCookManifestContractTest::RunTest(
    const FString& Parameters)
{
    const UDiscGolfRuntimeCookManifest* Manifest =
        LoadObject<UDiscGolfRuntimeCookManifest>(
            nullptr,
            TEXT("/Game/DiscGolf/Cook/DA_DG_RuntimeCookManifest."
                 "DA_DG_RuntimeCookManifest"));
    TestNotNull(TEXT("The canonical runtime cook manifest loads"), Manifest);
    if (!Manifest)
    {
        return false;
    }

    FString Error;
    const bool bValid = Manifest->ValidateRuntimeContract(Error);
    TestTrue(
        *FString::Printf(
            TEXT("The canonical runtime cook manifest satisfies its exact contract: %s"),
            *Error),
        bValid);
    return !HasAnyErrors();
}

#endif
