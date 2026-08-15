#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfMath.h"

namespace
{
    FThrowCommand MakeReleaseCommand(float TimingError, EThrowStyle Style = EThrowStyle::Backhand)
    {
        FThrowCommand Command;
        Command.Direction = FVector::ForwardVector;
        Command.Power01 = 0.82f;
        Command.HyzerDeg = 3.0f;
        Command.NoseAngleDeg = 1.0f;
        Command.LaunchAngleDeg = 7.0f;
        Command.TimingError = TimingError;
        Command.ThrowStyle = Style;
        return Command;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfTimingNormalizationTest,
    "DiscGolfTour.Release.TimingNormalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfTimingNormalizationTest::RunTest(const FString& Parameters)
{
    constexpr float Ideal = 0.82f;
    constexpr float Span = 0.18f;
    TestTrue(TEXT("Ideal needle position has zero timing error"),
        FMath::IsNearlyZero(DiscGolfMath::NormalizeTimingError(Ideal, Ideal, Span)));
    TestTrue(TEXT("Equal needle distance produces equal early and late magnitude"),
        FMath::IsNearlyEqual(
            DiscGolfMath::NormalizeTimingError(Ideal - 0.09f, Ideal, Span),
            -DiscGolfMath::NormalizeTimingError(Ideal + 0.09f, Ideal, Span)));
    TestTrue(TEXT("Timing error clamps at the early bound"),
        FMath::IsNearlyEqual(DiscGolfMath::NormalizeTimingError(0.0f, Ideal, Span), -1.0f));
    TestTrue(TEXT("Timing error reaches the late bound"),
        FMath::IsNearlyEqual(DiscGolfMath::NormalizeTimingError(1.0f, Ideal, Span), 1.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReleaseGradeBandsTest,
    "DiscGolfTour.Release.GradeBands",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReleaseGradeBandsTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Center is Perfect"), DiscGolfMath::ReleaseGrade(0.0f), EReleaseGrade::Perfect);
    TestEqual(TEXT("Perfect boundary is inclusive"), DiscGolfMath::ReleaseGrade(DiscGolfMath::ReleasePerfectError), EReleaseGrade::Perfect);
    TestEqual(TEXT("Great band follows Perfect"), DiscGolfMath::ReleaseGrade(0.20f), EReleaseGrade::Great);
    TestEqual(TEXT("Good band follows Great"), DiscGolfMath::ReleaseGrade(-0.50f), EReleaseGrade::Good);
    TestEqual(TEXT("Large miss is Poor"), DiscGolfMath::ReleaseGrade(0.90f), EReleaseGrade::Poor);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPerfectReleaseNeutralTest,
    "DiscGolfTour.Release.PerfectIsNeutral",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPerfectReleaseNeutralTest::RunTest(const FString& Parameters)
{
    const FThrowCommand Command = MakeReleaseCommand(0.0f);
    const FThrowRelease Release = DiscGolfMath::ResolveThrowRelease(Command);

    TestEqual(TEXT("Perfect timing receives Perfect grade"), Release.Grade, EReleaseGrade::Perfect);
    TestEqual(TEXT("Perfect timing is On Time"), Release.Timing, EReleaseTiming::OnTime);
    TestTrue(TEXT("Perfect quality is one"), FMath::IsNearlyEqual(Release.Quality01, 1.0f));
    TestTrue(TEXT("Perfect speed multiplier is neutral"), FMath::IsNearlyEqual(Release.SpeedMultiplier, 1.0f));
    TestTrue(TEXT("Perfect spin multiplier is neutral"), FMath::IsNearlyEqual(Release.SpinMultiplier, 1.0f));
    TestTrue(TEXT("Perfect release adds no aim error"), FMath::IsNearlyZero(Release.AimOffsetDeg));
    TestTrue(TEXT("Perfect release preserves hyzer"), FMath::IsNearlyEqual(Release.EffectiveHyzerDeg, Command.HyzerDeg));
    TestTrue(TEXT("Perfect release preserves nose"), FMath::IsNearlyEqual(Release.EffectiveNoseAngleDeg, Command.NoseAngleDeg));
    TestTrue(TEXT("Perfect release preserves launch angle"), FMath::IsNearlyEqual(Release.EffectiveLaunchAngleDeg, Command.LaunchAngleDeg));
    TestTrue(TEXT("Perfect RHBH speed preserves the v0.1 baseline"), FMath::IsNearlyEqual(Release.ReleaseSpeedMps, 26.54f, 0.001f));
    TestTrue(TEXT("Perfect RHBH spin preserves the v0.1 baseline"), FMath::IsNearlyEqual(Release.SpinRpm, 915.0f, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReleaseMissSymmetryTest,
    "DiscGolfTour.Release.EarlyLateSymmetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReleaseMissSymmetryTest::RunTest(const FString& Parameters)
{
    const FThrowRelease Early = DiscGolfMath::ResolveThrowRelease(MakeReleaseCommand(-0.75f));
    const FThrowRelease Late = DiscGolfMath::ResolveThrowRelease(MakeReleaseCommand(0.75f));

    TestEqual(TEXT("Negative miss is Early"), Early.Timing, EReleaseTiming::Early);
    TestEqual(TEXT("Positive miss is Late"), Late.Timing, EReleaseTiming::Late);
    TestEqual(TEXT("Equal misses share a grade"), Early.Grade, Late.Grade);
    TestTrue(TEXT("Equal misses have equal quality"), FMath::IsNearlyEqual(Early.Quality01, Late.Quality01));
    TestTrue(TEXT("Equal misses have equal release speed"), FMath::IsNearlyEqual(Early.ReleaseSpeedMps, Late.ReleaseSpeedMps));
    TestTrue(TEXT("Equal misses have equal spin"), FMath::IsNearlyEqual(Early.SpinRpm, Late.SpinRpm));
    TestTrue(TEXT("Aim errors oppose"), FMath::IsNearlyEqual(Early.AimOffsetDeg, -Late.AimOffsetDeg));
    TestTrue(TEXT("Hyzer errors oppose"), FMath::IsNearlyEqual(Early.HyzerOffsetDeg, -Late.HyzerOffsetDeg));
    TestTrue(TEXT("Nose errors oppose"), FMath::IsNearlyEqual(Early.NoseOffsetDeg, -Late.NoseOffsetDeg));
    TestTrue(TEXT("Launch errors oppose"), FMath::IsNearlyEqual(Early.LaunchOffsetDeg, -Late.LaunchOffsetDeg));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReleaseHandednessTest,
    "DiscGolfTour.Release.HandednessMirrorsAim",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReleaseHandednessTest::RunTest(const FString& Parameters)
{
    const FThrowRelease BackhandLate = DiscGolfMath::ResolveThrowRelease(MakeReleaseCommand(0.75f, EThrowStyle::Backhand));
    const FThrowRelease ForehandLate = DiscGolfMath::ResolveThrowRelease(MakeReleaseCommand(0.75f, EThrowStyle::Forehand));
    const FThrowRelease BackhandEarly = DiscGolfMath::ResolveThrowRelease(MakeReleaseCommand(-0.75f, EThrowStyle::Backhand));

    TestTrue(TEXT("Late RHBH misses player-right"), BackhandLate.AimOffsetDeg > 0.0f);
    TestTrue(TEXT("Early RHBH misses player-left"), BackhandEarly.AimOffsetDeg < 0.0f);
    TestTrue(TEXT("RHFH mirrors the same late timing miss"),
        FMath::IsNearlyEqual(BackhandLate.AimOffsetDeg, -ForehandLate.AimOffsetDeg));
    TestTrue(TEXT("Throw style does not reverse nose feedback"),
        FMath::IsNearlyEqual(BackhandLate.NoseOffsetDeg, ForehandLate.NoseOffsetDeg));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReleasePenaltyMonotonicTest,
    "DiscGolfTour.Release.PenaltiesAreMonotonicAndBounded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReleasePenaltyMonotonicTest::RunTest(const FString& Parameters)
{
    const FThrowRelease Great = DiscGolfMath::ResolveThrowRelease(MakeReleaseCommand(0.25f));
    const FThrowRelease Good = DiscGolfMath::ResolveThrowRelease(MakeReleaseCommand(0.50f));
    const FThrowRelease Poor = DiscGolfMath::ResolveThrowRelease(MakeReleaseCommand(1.0f));

    TestTrue(TEXT("Quality falls as error increases"), Great.Quality01 > Good.Quality01 && Good.Quality01 > Poor.Quality01);
    TestTrue(TEXT("Speed falls as error increases"), Great.ReleaseSpeedMps > Good.ReleaseSpeedMps && Good.ReleaseSpeedMps > Poor.ReleaseSpeedMps);
    TestTrue(TEXT("Spin falls as error increases"), Great.SpinRpm > Good.SpinRpm && Good.SpinRpm > Poor.SpinRpm);
    TestTrue(TEXT("Worst speed remains at the authored floor"), FMath::IsNearlyEqual(Poor.SpeedMultiplier, 0.84f, 0.001f));
    TestTrue(TEXT("Worst spin remains at the authored floor"), FMath::IsNearlyEqual(Poor.SpinMultiplier, 0.78f, 0.001f));
    TestTrue(TEXT("Worst aim offset remains bounded"), FMath::IsNearlyEqual(Poor.AimOffsetDeg, 6.0f, 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReleaseInputSanitizationTest,
    "DiscGolfTour.Release.InputSanitization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReleaseInputSanitizationTest::RunTest(const FString& Parameters)
{
    FThrowCommand Command = MakeReleaseCommand(4.0f);
    Command.Direction = FVector::ZeroVector;
    Command.Power01 = 4.0f;
    Command.HyzerDeg = 100.0f;
    Command.NoseAngleDeg = 100.0f;
    Command.LaunchAngleDeg = 100.0f;
    const FThrowRelease Release = DiscGolfMath::ResolveThrowRelease(Command);

    TestTrue(TEXT("Timing input is clamped"), FMath::IsNearlyEqual(Release.TimingError, 1.0f));
    TestTrue(TEXT("Power input is clamped before speed resolution"),
        FMath::IsNearlyEqual(Release.ReleaseSpeedMps, 30.5f * 0.84f, 0.001f));
    TestTrue(TEXT("Zero direction receives a safe fallback"), Release.Direction.Equals(FVector::ForwardVector));
    TestTrue(TEXT("Effective hyzer is clamped"), FMath::IsNearlyEqual(Release.EffectiveHyzerDeg, 34.0f));
    TestTrue(TEXT("Effective nose is clamped"), FMath::IsNearlyEqual(Release.EffectiveNoseAngleDeg, 11.0f));
    TestTrue(TEXT("Effective launch angle is clamped"), FMath::IsNearlyEqual(Release.EffectiveLaunchAngleDeg, 35.0f));
    return true;
}

#endif
