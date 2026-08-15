#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfBroadcastCameraMath.h"

namespace
{
FDiscBroadcastCameraInput MakeDriveCameraInput()
{
    FDiscBroadcastCameraInput Input;
    Input.ReleaseLocationCm = FVector(0.0f, 0.0f, 123.0f);
    Input.BasketLocationCm = FVector(11000.0f, 800.0f, 0.0f);
    Input.DiscLocationCm = Input.ReleaseLocationCm;
    Input.VelocityMps = FVector(25.0f, 1.5f, 2.0f);
    Input.ShotContext = EDiscShotContext::Drive;
    Input.GroundState = EDiscGroundState::Airborne;
    return Input;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBroadcastLaunchSelectionTest,
    "DiscGolfTour.Presentation.Camera.LaunchSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBroadcastLaunchSelectionTest::RunTest(const FString& Parameters)
{
    FDiscBroadcastCameraInput Input = MakeDriveCameraInput();
    Input.ElapsedSeconds = 0.4f;
    Input.DiscLocationCm.X = 700.0f;
    TestEqual(TEXT("Early flight uses the launch camera"),
        DiscGolfBroadcastCameraMath::SelectMode(Input), EDiscBroadcastCameraMode::Launch);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBroadcastFairwaySelectionTest,
    "DiscGolfTour.Presentation.Camera.FairwaySelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBroadcastFairwaySelectionTest::RunTest(const FString& Parameters)
{
    FDiscBroadcastCameraInput Input = MakeDriveCameraInput();
    Input.ElapsedSeconds = 2.2f;
    Input.DiscLocationCm = FVector(4300.0f, -500.0f, 850.0f);
    Input.VelocityMps = FVector(20.0f, -2.0f, 0.3f);
    TestEqual(TEXT("Mid-flight drive uses long-lens fairway tracking"),
        DiscGolfBroadcastCameraMath::SelectMode(Input), EDiscBroadcastCameraMode::Fairway);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBroadcastFinishSelectionTest,
    "DiscGolfTour.Presentation.Camera.FinishSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBroadcastFinishSelectionTest::RunTest(const FString& Parameters)
{
    FDiscBroadcastCameraInput NearBasket = MakeDriveCameraInput();
    NearBasket.ElapsedSeconds = 5.4f;
    NearBasket.DiscLocationCm = FVector(8500.0f, -400.0f, 500.0f);
    NearBasket.VelocityMps = FVector(13.0f, 0.0f, -0.2f);
    TestEqual(TEXT("Basket approach selects the finish camera"),
        DiscGolfBroadcastCameraMath::SelectMode(NearBasket), EDiscBroadcastCameraMode::Finish);

    FDiscBroadcastCameraInput Landing = MakeDriveCameraInput();
    Landing.ElapsedSeconds = 4.0f;
    Landing.DiscLocationCm = FVector(6000.0f, -2400.0f, 420.0f);
    Landing.VelocityMps = FVector(14.0f, -4.0f, -1.8f);
    TestEqual(TEXT("Low descending miss selects the landing-zone finish camera"),
        DiscGolfBroadcastCameraMath::SelectMode(Landing), EDiscBroadcastCameraMode::Finish);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBroadcastPuttingSelectionTest,
    "DiscGolfTour.Presentation.Camera.PuttingSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBroadcastPuttingSelectionTest::RunTest(const FString& Parameters)
{
    FDiscBroadcastCameraInput Input = MakeDriveCameraInput();
    Input.ShotContext = EDiscShotContext::Circle2Putt;
    Input.BasketLocationCm = FVector(1400.0f, 0.0f, 0.0f);
    Input.ElapsedSeconds = 0.10f;
    Input.DiscLocationCm.X = 80.0f;
    TestEqual(TEXT("Putting briefly preserves launch readability"),
        DiscGolfBroadcastCameraMath::SelectMode(Input), EDiscBroadcastCameraMode::Launch);
    Input.ElapsedSeconds = 0.35f;
    Input.DiscLocationCm.X = 320.0f;
    TestEqual(TEXT("Putting cuts directly to the basket camera"),
        DiscGolfBroadcastCameraMath::SelectMode(Input), EDiscBroadcastCameraMode::Finish);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBroadcastFramingTest,
    "DiscGolfTour.Presentation.Camera.Framing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBroadcastFramingTest::RunTest(const FString& Parameters)
{
    const FDiscBroadcastCameraInput Input = MakeDriveCameraInput();
    const FDiscBroadcastCameraPlan Launch = DiscGolfBroadcastCameraMath::BuildPlan(
        Input, EDiscBroadcastCameraMode::Launch);
    const FDiscBroadcastCameraPlan Fairway = DiscGolfBroadcastCameraMath::BuildPlan(
        Input, EDiscBroadcastCameraMode::Fairway);
    const FDiscBroadcastCameraPlan Finish = DiscGolfBroadcastCameraMath::BuildPlan(
        Input, EDiscBroadcastCameraMode::Finish);
    FDiscBroadcastCameraInput PuttingInput = Input;
    PuttingInput.ShotContext = EDiscShotContext::Circle2Putt;
    const FDiscBroadcastCameraPlan PuttingFinish = DiscGolfBroadcastCameraMath::BuildPlan(
        PuttingInput, EDiscBroadcastCameraMode::Finish);

    TestTrue(TEXT("Every camera plan has a finite location"),
        !Launch.WorldLocationCm.ContainsNaN() && !Fairway.WorldLocationCm.ContainsNaN()
        && !Finish.WorldLocationCm.ContainsNaN());
    TestTrue(TEXT("Every plan has a usable look direction"),
        FVector::DistSquared(Launch.WorldLocationCm, Launch.LookAtWorldCm) > 1.0f
        && FVector::DistSquared(Fairway.WorldLocationCm, Fairway.LookAtWorldCm) > 1.0f
        && FVector::DistSquared(Finish.WorldLocationCm, Finish.LookAtWorldCm) > 1.0f);
    TestTrue(TEXT("Long-lens fairway FOV is narrower than launch"),
        Fairway.FieldOfViewDeg < Launch.FieldOfViewDeg);
    TestTrue(TEXT("Putting finish uses wider context than the drive landing camera"),
        PuttingFinish.FieldOfViewDeg > Finish.FieldOfViewDeg);
    TestTrue(TEXT("Three modes produce distinct camera positions"),
        !Launch.WorldLocationCm.Equals(Fairway.WorldLocationCm, 1.0f)
        && !Fairway.WorldLocationCm.Equals(Finish.WorldLocationCm, 1.0f));
    return true;
}

#endif
