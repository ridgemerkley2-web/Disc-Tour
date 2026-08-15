#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPuttingContextTest,
    "DiscGolfTour.Putting.ContextFromLie",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPuttingContextTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Circle 1 selects the short putting model"),
        DiscGolfMath::ShotContextForLie(ELieType::Circle1), EDiscShotContext::Circle1Putt);
    TestEqual(TEXT("Circle 2 selects the long putting model"),
        DiscGolfMath::ShotContextForLie(ELieType::Circle2), EDiscShotContext::Circle2Putt);
    TestEqual(TEXT("Fairway preserves the drive model"),
        DiscGolfMath::ShotContextForLie(ELieType::Fairway), EDiscShotContext::Drive);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPuttingPowerTest,
    "DiscGolfTour.Putting.PowerAndReleaseScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPuttingPowerTest::RunTest(const FString& Parameters)
{
    const float SevenMeterPower = DiscGolfMath::RecommendedPuttPower01(7.0f);
    const float FourteenMeterPower = DiscGolfMath::RecommendedPuttPower01(14.0f);
    TestTrue(TEXT("Longer putts receive more recommended pace"), FourteenMeterPower > SevenMeterPower);
    TestTrue(TEXT("Power/range preview round-trips at seven meters"),
        FMath::IsNearlyEqual(DiscGolfMath::EstimatedPuttRangeMeters(SevenMeterPower), 7.0f, 0.01f));

    FThrowCommand Command;
    Command.ShotContext = EDiscShotContext::Circle1Putt;
    Command.Power01 = SevenMeterPower;
    Command.TimingError = 0.0f;
    const FThrowRelease Release = DiscGolfMath::ResolveThrowRelease(Command);
    TestEqual(TEXT("Release retains its putting context"), Release.ShotContext, EDiscShotContext::Circle1Putt);
    TestTrue(TEXT("Putting uses a controlled speed band"), Release.ReleaseSpeedMps > 8.0f && Release.ReleaseSpeedMps < 10.0f);
    TestTrue(TEXT("Putting spin is below the drive baseline"), Release.SpinRpm < 620.0f);

    FThrowCommand Circle2Command = Command;
    Circle2Command.ShotContext = EDiscShotContext::Circle2Putt;
    Circle2Command.Power01 = FourteenMeterPower;
    const FThrowRelease Circle2Release = DiscGolfMath::ResolveThrowRelease(Circle2Command);
    TestTrue(TEXT("Circle 2 calibrated release stays controlled"),
        FMath::IsNearlyEqual(Circle2Release.ReleaseSpeedMps, 11.1854f, 0.001f));
    TestTrue(TEXT("Circle 2 receives more pace than Circle 1"),
        Circle2Release.ReleaseSpeedMps > Release.ReleaseSpeedMps);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPuttingAimReadTest,
    "DiscGolfTour.Putting.AimRead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPuttingAimReadTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Centered aim reads zero"),
        FMath::IsNearlyZero(DiscGolfMath::SignedAimErrorDeg(FVector::ForwardVector, FVector::ForwardVector)));
    TestTrue(TEXT("Opposite target sides produce opposite read"),
        FMath::IsNearlyEqual(
            DiscGolfMath::SignedAimErrorDeg(FVector::ForwardVector, FVector(10.0f, 1.0f, 0.0f)),
            -DiscGolfMath::SignedAimErrorDeg(FVector::ForwardVector, FVector(10.0f, -1.0f, 0.0f))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBasketCenterAndWeakChainsTest,
    "DiscGolfTour.Putting.Basket.CenterAndWeakChains",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBasketCenterAndWeakChainsTest::RunTest(const FString& Parameters)
{
    const FBasketContactEvaluation Center = DiscGolfMath::EvaluateBasketContact(
        FVector(-52.0f, 0.0f, 114.0f), FVector(8.0f, 0.0f, 0.0f));
    const FBasketContactEvaluation WeakSide = DiscGolfMath::EvaluateBasketContact(
        FVector(-52.0f, 36.0f, 114.0f), FVector(8.0f, 0.0f, 0.0f));
    const FBasketContactEvaluation TooFast = DiscGolfMath::EvaluateBasketContact(
        FVector(-52.0f, 0.0f, 114.0f), FVector(15.0f, 0.0f, 0.0f));

    TestEqual(TEXT("Center-chain pace is caught"), Center.Result, EBasketContactResult::Caught);
    TestEqual(TEXT("Outer chains deflect"), WeakSide.Result, EBasketContactResult::ChainDeflection);
    TestEqual(TEXT("Excess pace spits through the chains"), TooFast.Result, EBasketContactResult::ChainDeflection);
    TestTrue(TEXT("Weak-chain response loses most speed"), WeakSide.DeflectedVelocityMps.Size() < 3.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBasketMetalRejectionTest,
    "DiscGolfTour.Putting.Basket.BandAndTray",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBasketMetalRejectionTest::RunTest(const FString& Parameters)
{
    const FBasketContactEvaluation Band = DiscGolfMath::EvaluateBasketContact(
        FVector(-52.0f, 0.0f, 150.0f), FVector(8.0f, 0.0f, 0.0f));
    const FBasketContactEvaluation Tray = DiscGolfMath::EvaluateBasketContact(
        FVector(-52.0f, 0.0f, 84.0f), FVector(8.0f, 0.0f, 0.0f));

    TestEqual(TEXT("High putt rejects from top band"), Band.Result, EBasketContactResult::BandRejection);
    TestEqual(TEXT("Low putt rejects from tray"), Tray.Result, EBasketContactResult::TrayRejection);
    TestTrue(TEXT("Band sends the disc downward"), Band.DeflectedVelocityMps.Z < 0.0f);
    TestTrue(TEXT("Tray sends the disc upward"), Tray.DeflectedVelocityMps.Z > 0.0f);
    return true;
}

#endif
