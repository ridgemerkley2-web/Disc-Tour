#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfHudPresentationState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHudPresentationStateTest,
    "DiscGolfTour.Presentation.HudState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHudPresentationStateTest::RunTest(const FString& Parameters)
{
    using namespace DiscGolfHudPresentation;

    const FDiscGolfHudPresentationState Play = Resolve({});
    TestTrue(TEXT("Play shows current lie"), Play.bShowCurrentLie);
    TestTrue(TEXT("Play shows actionable setup"), Play.bShowShotSetup);
    TestTrue(TEXT("Play shows help"), Play.bShowHelp);

    FDiscGolfHudPresentationContext FlightContext;
    FlightContext.bActiveDisc = true;
    FlightContext.bHasLastRelease = true;
    FlightContext.LastReleaseAgeSeconds = 1.0f;
    const FDiscGolfHudPresentationState Flight = Resolve(FlightContext);
    TestTrue(TEXT("Active disc selects flight mode"), Flight.Mode == EDiscGolfHudPresentationMode::Flight);
    TestTrue(TEXT("Flight shows live strip"), Flight.bShowFlightStrip);
    TestTrue(TEXT("Recent release shows transient feedback during flight"), Flight.bShowShotFeedback);
    TestFalse(TEXT("Flight hides setup"), Flight.bShowShotSetup);
    TestFalse(TEXT("Flight hides help"), Flight.bShowHelp);

    FlightContext.LastReleaseAgeSeconds = ShotFeedbackDurationSeconds + 0.01f;
    TestFalse(TEXT("Shot feedback expires while flight continues"), Resolve(FlightContext).bShowShotFeedback);

    FDiscGolfHudPresentationContext ReviewContext;
    ReviewContext.bHasLastRelease = true;
    ReviewContext.LastReleaseAgeSeconds = 2.0f;
    const FDiscGolfHudPresentationState Review = Resolve(ReviewContext);
    TestTrue(TEXT("Recent settled release selects review mode"), Review.Mode == EDiscGolfHudPresentationMode::Review);
    TestTrue(TEXT("Review keeps lie context"), Review.bShowCurrentLie);
    TestTrue(TEXT("Review shows feedback"), Review.bShowShotFeedback);
    TestFalse(TEXT("Review temporarily hides setup"), Review.bShowShotSetup);

    FDiscGolfHudPresentationContext ReplayContext = FlightContext;
    ReplayContext.bReplayActive = true;
    ReplayContext.LastReleaseAgeSeconds = 1.0f;
    const FDiscGolfHudPresentationState Replay = Resolve(ReplayContext);
    TestTrue(TEXT("Replay takes precedence"), Replay.Mode == EDiscGolfHudPresentationMode::Replay);
    TestTrue(TEXT("Replay shows replay chrome"), Replay.bShowReplayChrome);
    TestFalse(TEXT("Replay hides live strip"), Replay.bShowFlightStrip);
    TestFalse(TEXT("Replay hides shot feedback"), Replay.bShowShotFeedback);

    FDiscGolfHudPresentationContext CompleteContext;
    CompleteContext.bComplete = true;
    const FDiscGolfHudPresentationState Complete = Resolve(CompleteContext);
    TestTrue(TEXT("Completion selects complete mode"), Complete.Mode == EDiscGolfHudPresentationMode::Complete);
    TestTrue(TEXT("Completion keeps status context"), Complete.bShowCurrentLie);
    TestFalse(TEXT("Completion hides setup"), Complete.bShowShotSetup);
    TestFalse(TEXT("Completion hides help"), Complete.bShowHelp);

    return true;
}

#endif
