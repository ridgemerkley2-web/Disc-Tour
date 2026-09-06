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

    const auto CanCommit = [](bool bBaseLaunchAllowed = true,
        bool bMainMenuVisible = false,
        bool bRegressionActive = false,
        bool bLieTransitionActive = false,
        bool bHasPlayerController = true,
        bool bPresentationDismissReleasePending = false,
        bool bFreshThrowDownRequiredAfterPresentation = false,
        bool bControlsMenuOpen = false,
        bool bCharacterCreatorOpen = false,
        bool bWorldPaused = false)
    {
        return DiscGolfGameplayGate::CanCommitPlayerRelease(
            bBaseLaunchAllowed, bMainMenuVisible, bRegressionActive,
            bLieTransitionActive, bHasPlayerController,
            bPresentationDismissReleasePending,
            bFreshThrowDownRequiredAfterPresentation,
            bControlsMenuOpen, bCharacterCreatorOpen, bWorldPaused);
    };
    TestTrue(TEXT("Committed animation release keeps the normal player gate open"), CanCommit());
    TestFalse(TEXT("Committed release cannot bypass the base launch gate"),
        CanCommit(false));
    TestFalse(TEXT("Committed release cannot launch behind the main menu"),
        CanCommit(true, true));
    TestFalse(TEXT("Committed release cannot launch during regression"),
        CanCommit(true, false, true));
    TestFalse(TEXT("Committed release cannot launch during lie transition"),
        CanCommit(true, false, false, true));
    TestFalse(TEXT("Committed release requires the player controller authority"),
        CanCommit(true, false, false, false, false));
    TestFalse(TEXT("Presentation-dismiss release cannot become a throw"),
        CanCommit(true, false, false, false, true, true));
    TestFalse(TEXT("A fresh physical throw press remains mandatory after presentation"),
        CanCommit(true, false, false, false, true, false, true));
    TestFalse(TEXT("Committed release cannot launch behind the controls menu"),
        CanCommit(true, false, false, false, true, false, false, true));
    TestFalse(TEXT("Committed release cannot launch behind the character creator"),
        CanCommit(true, false, false, false, true, false, false, false, true));
    TestFalse(TEXT("Committed release cannot launch while the world is paused"),
        CanCommit(true, false, false, false, true, false, false, false, false, true));
    return true;
}

#endif
