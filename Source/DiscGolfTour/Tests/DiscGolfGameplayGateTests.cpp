#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfGameplayGate.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfThrowLifecycleGateTest,
    "DiscGolfTour.Gameplay.ThrowLifecycleGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfThrowLifecycleGateTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Clean tee state can launch"), DiscGolfGameplayGate::CanLaunchThrow(
        true, false, false, false, false, false));
    TestFalse(TEXT("Missing hole blocks launch"), DiscGolfGameplayGate::CanLaunchThrow(
        false, false, false, false, false, false));
    TestFalse(TEXT("Active regression disc cannot be replaced"), DiscGolfGameplayGate::CanLaunchThrow(
        true, true, false, false, false, false));
    TestFalse(TEXT("Replay blocks launch"), DiscGolfGameplayGate::CanLaunchThrow(
        true, false, true, false, false, false));
    TestFalse(TEXT("Flyover blocks launch"), DiscGolfGameplayGate::CanLaunchThrow(
        true, false, false, true, false, false));
    TestFalse(TEXT("Completed hole blocks launch"), DiscGolfGameplayGate::CanLaunchThrow(
        true, false, false, false, true, false));
    TestFalse(TEXT("Scorecard blocks launch"), DiscGolfGameplayGate::CanLaunchThrow(
        true, false, false, false, false, true));
    TestFalse(TEXT("Hole introduction blocks launch until skipped or completed"),
        DiscGolfGameplayGate::CanLaunchThrow(true, false, false, false, false, false, true));
    return true;
}

#endif
