#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEffectiveDistanceFlatTest,
    "DiscGolfTour.Course.EffectiveDistance.Flat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEffectiveDistanceFlatTest::RunTest(const FString& Parameters)
{
    const FVector Tee(0, 0, 0);
    const FVector Basket(3048, 0, 0); // 100 ft
    TestTrue(TEXT("Flat 100 ft hole remains 100 effective feet"), FMath::IsNearlyEqual(DiscGolfMath::EffectiveHoleDistanceFeet(Tee, Basket), 100.0f, 0.01f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEffectiveDistanceUphillTest,
    "DiscGolfTour.Course.EffectiveDistance.Uphill",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEffectiveDistanceUphillTest::RunTest(const FString& Parameters)
{
    const FVector Tee(0, 0, 0);
    const FVector Basket(3048, 0, 304.8f); // 100 ft horizontal-ish plus 10 ft uphill
    const float Result = DiscGolfMath::EffectiveHoleDistanceFeet(Tee, Basket);
    TestTrue(TEXT("Uphill adds meaningful effective distance"), Result > 129.0f && Result < 131.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStabilityMomentOppositionTest,
    "DiscGolfTour.Physics.StabilityMoment.TurnFadeOppose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStabilityMomentOppositionTest::RunTest(const FString& Parameters)
{
    const float HighSpeed = DiscGolfMath::DiscStabilityMomentNm(28.0f, 0.0035f, 0.0075f, 20.0f, 17.0f);
    const float LowSpeed = DiscGolfMath::DiscStabilityMomentNm(9.0f, 0.0035f, 0.0075f, 20.0f, 17.0f);
    TestTrue(TEXT("High-speed turn moment uses the positive v0.1 convention"), HighSpeed > 0.0f);
    TestTrue(TEXT("Low-speed fade moment opposes turn"), LowSpeed < 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStabilityMomentNeutralBandTest,
    "DiscGolfTour.Physics.StabilityMoment.NeutralBand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStabilityMomentNeutralBandTest::RunTest(const FString& Parameters)
{
    const float MidSpeed = DiscGolfMath::DiscStabilityMomentNm(18.5f, 0.0035f, 0.0075f, 20.0f, 17.0f);
    TestTrue(TEXT("Between turn/fade thresholds the calibration moment can be neutral"), FMath::IsNearlyZero(MidSpeed));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfLaunchDirectionConventionTest,
    "DiscGolfTour.Physics.LaunchDirection.PositiveIsUp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfLaunchDirectionConventionTest::RunTest(const FString& Parameters)
{
    const FVector Upward = DiscGolfMath::LaunchDirectionFromFlat(FVector::ForwardVector, 7.0f);
    const FVector Downward = DiscGolfMath::LaunchDirectionFromFlat(FVector::ForwardVector, -5.0f);
    TestTrue(TEXT("A positive gameplay launch angle produces upward velocity"), Upward.Z > 0.0f);
    TestTrue(TEXT("A negative gameplay launch angle produces downward velocity"), Downward.Z < 0.0f);
    TestTrue(TEXT("Launch direction remains normalized"), Upward.IsNormalized());
    return true;
}

#endif
