#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DiscGolfHoleAuthoringUtility.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHoleAuthoringValidationTest,
    "DiscGolfTour.CourseAuthoring.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHoleAuthoringValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfHoleAuthoringDraft Draft = UDiscGolfHoleAuthoringUtility::CreateHoleDefinition(
        TEXT("TestCourse"), TEXT("Championship"), 1, FText::FromString(TEXT("Opening")));
    FDiscGolfHoleValidationResult Missing = UDiscGolfHoleAuthoringUtility::ValidateHole(Draft, {});
    TestFalse(TEXT("Unassigned draft is rejected"), Missing.bValid);
    TestTrue(TEXT("Missing tee is explicit"), Missing.Errors.Contains(TEXT("Missing tee assignment")));

    UDiscGolfHoleAuthoringUtility::AssignTee(Draft, FVector::ZeroVector);
    UDiscGolfHoleAuthoringUtility::AssignBasket(Draft, FVector(11000.0f, 800.0f, 80.0f));
    UDiscGolfHoleAuthoringUtility::DrawFairwaySpline(Draft,
        {Draft.Definition.TeeLocationCm, FVector(6000.0f, 200.0f, 40.0f), Draft.Definition.BasketLocationCm});
    Draft.Definition.CameraAnchors.Add({TEXT("Intro"), EDiscGolfCameraAnchorMode::Launch, FVector(-400, 800, 300), 60.0f});
    UDiscGolfHoleAuthoringUtility::AssignIntroCamera(Draft, TEXT("Intro"));
    UDiscGolfHoleAuthoringUtility::AssignPreviewSpline(Draft,
        {FVector(-500, 0, 400), FVector(6000, 200, 1000), FVector(11000, 800, 500)});
    const FDiscGolfHoleValidationResult Ready = UDiscGolfHoleAuthoringUtility::ValidateHole(Draft, {});
    TestTrue(TEXT("Complete hand-authored draft validates"), Ready.bValid);
    TestEqual(TEXT("Distance display rounds correctly"), Ready.DisplayDistanceFeet, 362);

    const FDiscGolfHoleValidationResult Duplicate = UDiscGolfHoleAuthoringUtility::ValidateHole(
        Draft, {Draft.Definition});
    TestTrue(TEXT("Duplicate number is rejected"), Duplicate.Errors.Contains(TEXT("Duplicate hole number")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHole1ValidationReportTest,
    "DiscGolfTour.CourseAuthoring.PineRidgeHole1Report",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHole1ValidationReportTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FString Path;
    FString Error;
    TestTrue(TEXT("Hole 1 benchmark report generates"),
        UDiscGolfHoleAuthoringUtility::GeneratePineRidgeHole1ValidationReport(Path, Error));
    TestTrue(TEXT("Report uses required Saved/CourseReports destination"),
        Path.EndsWith(TEXT("Saved/CourseReports/PineRidgeHole1Validation.json"))
        || Path.EndsWith(TEXT("Saved\\CourseReports\\PineRidgeHole1Validation.json")));
    TestTrue(TEXT("Report exists"), IFileManager::Get().FileExists(*Path));
    return true;
}

#endif
