#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfMath.h"

#include <limits>

namespace
{
    FThrowCommand MakeReleaseCommand(
        float TimingError,
        EThrowStyle Style = EThrowStyle::Backhand,
        EDGHandedness Handedness = EDGHandedness::Right)
    {
        FThrowCommand Command;
        Command.MoldId = TEXT("Apex");
        Command.Direction = FVector::ForwardVector;
        Command.Power01 = 0.82f;
        Command.HyzerDeg = 3.0f;
        Command.NoseAngleDeg = 1.0f;
        Command.LaunchAngleDeg = 7.0f;
        Command.TimingError = TimingError;
        Command.ThrowStyle = Style;
        Command.Handedness = Handedness;
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
    const FThrowRelease RHBH = DiscGolfMath::ResolveThrowRelease(
        MakeReleaseCommand(0.75f, EThrowStyle::Backhand, EDGHandedness::Right));
    const FThrowRelease RHFH = DiscGolfMath::ResolveThrowRelease(
        MakeReleaseCommand(0.75f, EThrowStyle::Forehand, EDGHandedness::Right));
    const FThrowRelease LHBH = DiscGolfMath::ResolveThrowRelease(
        MakeReleaseCommand(0.75f, EThrowStyle::Backhand, EDGHandedness::Left));
    const FThrowRelease LHFH = DiscGolfMath::ResolveThrowRelease(
        MakeReleaseCommand(0.75f, EThrowStyle::Forehand, EDGHandedness::Left));

    TestTrue(TEXT("Late RHBH misses player-right"), RHBH.AimOffsetDeg > 0.0f);
    TestTrue(TEXT("Late RHFH mirrors RHBH"),
        FMath::IsNearlyEqual(RHBH.AimOffsetDeg, -RHFH.AimOffsetDeg));
    TestTrue(TEXT("Late LHBH mirrors RHBH"),
        FMath::IsNearlyEqual(RHBH.AimOffsetDeg, -LHBH.AimOffsetDeg));
    TestTrue(TEXT("Late LHFH shares the RHBH rotation convention"),
        FMath::IsNearlyEqual(RHBH.AimOffsetDeg, LHFH.AimOffsetDeg));
    TestTrue(TEXT("Backhand speed magnitude is independent of throwing hand"),
        FMath::IsNearlyEqual(RHBH.ReleaseSpeedMps, LHBH.ReleaseSpeedMps));
    TestTrue(TEXT("Forehand spin magnitude is independent of throwing hand"),
        FMath::IsNearlyEqual(RHFH.SpinRpm, LHFH.SpinRpm));
    TestTrue(TEXT("Style and hand do not reverse nose feedback"),
        FMath::IsNearlyEqual(RHBH.NoseOffsetDeg, LHFH.NoseOffsetDeg));
    TestEqual(TEXT("RHBH release preserves right-handed provenance"),
        RHBH.Handedness, EDGHandedness::Right);
    TestEqual(TEXT("LHBH release preserves left-handed provenance"),
        LHBH.Handedness, EDGHandedness::Left);
    TestTrue(TEXT("RHBH rotation sign is positive"),
        DiscGolfMath::ThrowRotationSign(EThrowStyle::Backhand, EDGHandedness::Right) > 0.0f);
    TestTrue(TEXT("RHFH rotation sign is negative"),
        DiscGolfMath::ThrowRotationSign(EThrowStyle::Forehand, EDGHandedness::Right) < 0.0f);
    TestTrue(TEXT("LHBH rotation sign is negative"),
        DiscGolfMath::ThrowRotationSign(EThrowStyle::Backhand, EDGHandedness::Left) < 0.0f);
    TestTrue(TEXT("LHFH rotation sign is positive"),
        DiscGolfMath::ThrowRotationSign(EThrowStyle::Forehand, EDGHandedness::Left) > 0.0f);
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

    FThrowCommand SlopedAim = MakeReleaseCommand(0.0f);
    SlopedAim.Direction = FVector(3.0f, 4.0f, 12.0f);
    const FThrowRelease CanonicalAim = DiscGolfMath::ResolveThrowRelease(SlopedAim);
    TestTrue(TEXT("Release provenance canonicalizes aim to the simulated horizontal direction"),
        CanonicalAim.Direction.Equals(FVector(0.6f, 0.8f, 0.0f), KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGripOriginAimPointPreservationTest,
    "DiscGolfTour.Physics.ReleaseOrigin.AimPointPreservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGripOriginAimPointPreservationTest::RunTest(const FString& Parameters)
{
    struct FAimRebaseFixture
    {
        const TCHAR* Label;
        FVector GripLocationCm;
        float AimReferenceDistanceCm;
        float AimOffsetDeg;
        float ExpectedBaseYawDeg;
        FVector AimLineOriginCm = FVector::ZeroVector;
        FVector BaseDirection = FVector::ForwardVector;
    };

    const FAimRebaseFixture Fixtures[] = {
        { TEXT("Drive at 55 m"), FVector(4.345f, 70.069f, 33.687f), 5500.0f, 0.0f, -0.730475f },
        { TEXT("Approach at 20.001 m"), FVector(3.441f, 71.163f, 33.095f), 2000.1f, 0.0f, -2.041217f },
        { TEXT("Approach at 55 m"), FVector(3.441f, 71.163f, 33.095f), 5500.0f, 0.0f, -0.741757f },
        { TEXT("Latent putt at four feet"), FVector(-7.994f, 68.765f, 29.694f), 121.92f, 0.0f, -27.892793f },
        { TEXT("Latent putt at five feet"), FVector(-7.994f, 68.765f, 29.694f), 152.4f, 0.0f, -23.206049f },
        { TEXT("Drive at maximum early aim offset"), FVector(4.345f, 70.069f, 33.687f), 5500.0f, -6.0f, -0.730232f },
        { TEXT("Drive at maximum late aim offset"), FVector(4.345f, 70.069f, 33.687f), 5500.0f, 6.0f, -0.722700f },
        { TEXT("Mirrored drive grip"), FVector(4.345f, -70.069f, 33.687f), 5500.0f, 0.0f, 0.730475f },
        { TEXT("Grip already on the aim line"), FVector(70.0f, 0.0f, 35.0f), 5500.0f, 0.0f, 0.0f },
        {
            TEXT("Translated large-world drive"),
            FVector(50000004.345, -34999929.931, 1233.687),
            5500.0f,
            0.0f,
            -0.730475f,
            FVector(50000000.0, -35000000.0, 1200.0),
            FVector::ForwardVector
        },
        {
            TEXT("Rotated non-unit finite-Z drive"),
            FVector(42000000.0 - 38.698505566, -27000000.0 + 58.574477829, 933.687),
            5500.0f,
            3.0f,
            36.272411f,
            FVector(42000000.0, -27000000.0, 900.0),
            FVector(5.590448570, 4.212705162, 5.0)
        },
    };

    for (const FAimRebaseFixture& Fixture : Fixtures)
    {
        FVector RebasedDirection = FVector::ZeroVector;
        FString Error = TEXT("prepopulated error");
        const FVector FlatBaseDirection(
            Fixture.BaseDirection.X, Fixture.BaseDirection.Y, 0.0f);
        const FVector NormalizedBaseDirection = FlatBaseDirection.GetSafeNormal();
        const bool bRebased = DiscGolfMath::TryRebaseReleaseDirectionToPreserveAimPoint(
            Fixture.AimLineOriginCm,
            Fixture.GripLocationCm,
            Fixture.AimReferenceDistanceCm,
            Fixture.BaseDirection,
            Fixture.AimOffsetDeg,
            RebasedDirection,
            Error);

        const FString SuccessLabel = FString::Printf(
            TEXT("%s resolves a rebased direction"), Fixture.Label);
        TestTrue(*SuccessLabel, bRebased);
        if (!bRebased)
        {
            continue;
        }
        const FString ErrorLabel = FString::Printf(
            TEXT("%s clears its error"), Fixture.Label);
        TestTrue(*ErrorLabel, Error.IsEmpty());
        const FString UnitLabel = FString::Printf(
            TEXT("%s returns a unit horizontal direction"), Fixture.Label);
        TestTrue(*UnitLabel,
            FMath::IsNearlyEqual(RebasedDirection.SizeSquared(), 1.0, 1.0e-5)
            && FMath::IsNearlyZero(
                RebasedDirection.Z, static_cast<double>(KINDA_SMALL_NUMBER)));

        const double ActualBaseYawDeg = FMath::RadiansToDegrees(
            FMath::Atan2(RebasedDirection.Y, RebasedDirection.X));
        const FString YawLabel = FString::Printf(
            TEXT("%s matches the measured base-yaw correction"), Fixture.Label);
        TestTrue(*YawLabel,
            FMath::IsNearlyEqual(
                ActualBaseYawDeg,
                static_cast<double>(Fixture.ExpectedBaseYawDeg),
                0.001));

        const FQuat AimError(
            FVector::UpVector,
            FMath::DegreesToRadians(Fixture.AimOffsetDeg));
        const FVector OriginalPostTimingDirection =
            AimError.RotateVector(NormalizedBaseDirection).GetSafeNormal();
        const FVector AimPointCm = Fixture.AimLineOriginCm
            + OriginalPostTimingDirection * Fixture.AimReferenceDistanceCm;
        const FVector ReleaseToAimPoint(
            AimPointCm.X - Fixture.GripLocationCm.X,
            AimPointCm.Y - Fixture.GripLocationCm.Y,
            0.0f);
        const FVector ExpectedPostTimingDirection =
            ReleaseToAimPoint.GetSafeNormal();
        const FVector ActualPostTimingDirection =
            AimError.RotateVector(RebasedDirection).GetSafeNormal();
        const FString DirectionLabel = FString::Printf(
            TEXT("%s preserves the post-timing aim-point direction"), Fixture.Label);
        TestTrue(*DirectionLabel,
            ActualPostTimingDirection.Equals(
                ExpectedPostTimingDirection, 1.0e-5f));

        const FVector PlanarGripLocation(
            Fixture.GripLocationCm.X, Fixture.GripLocationCm.Y, AimPointCm.Z);
        const double TravelToAimPointCm = FVector::DotProduct(
            AimPointCm - PlanarGripLocation, ActualPostTimingDirection);
        const FVector ClosestPointOnReleaseRay = PlanarGripLocation
            + ActualPostTimingDirection * TravelToAimPointCm;
        const FString ForwardLabel = FString::Printf(
            TEXT("%s keeps the preserved aim point ahead of the grip"), Fixture.Label);
        TestTrue(*ForwardLabel, TravelToAimPointCm > 0.0);
        const FString MissLabel = FString::Printf(
            TEXT("%s release ray hits the preserved aim point within 0.1 cm"), Fixture.Label);
        TestTrue(*MissLabel,
            FVector::Dist2D(ClosestPointOnReleaseRay, AimPointCm) <= 0.1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGripOriginAimPointFailClosedTest,
    "DiscGolfTour.Physics.ReleaseOrigin.FailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGripOriginAimPointFailClosedTest::RunTest(const FString& Parameters)
{
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    const FVector ValidReleaseOrigin(4.345f, 70.069f, 33.687f);
    const FVector SentinelOutput(0.25f, 0.5f, 0.75f);
    const auto RejectsAtomically = [this, &SentinelOutput](
        const TCHAR* Label,
        const FVector& AimLineOriginCm,
        const FVector& ReleaseOriginCm,
        float AimReferenceDistanceCm,
        const FVector& BaseDirection,
        float AimOffsetDeg)
    {
        FVector Output = SentinelOutput;
        FString Error;
        const bool bResult = DiscGolfMath::TryRebaseReleaseDirectionToPreserveAimPoint(
            AimLineOriginCm,
            ReleaseOriginCm,
            AimReferenceDistanceCm,
            BaseDirection,
            AimOffsetDeg,
            Output,
            Error);
        const FString RejectLabel = FString::Printf(TEXT("%s is rejected"), Label);
        TestFalse(*RejectLabel, bResult);
        const FString ErrorLabel = FString::Printf(TEXT("%s reports an error"), Label);
        TestFalse(*ErrorLabel, Error.IsEmpty());
        const FString AtomicLabel = FString::Printf(
            TEXT("%s leaves the output unchanged"), Label);
        TestTrue(*AtomicLabel, Output.Equals(SentinelOutput, 0.0f));
    };

    RejectsAtomically(TEXT("NaN aim-line origin"),
        FVector(NaN, 0.0f, 0.0f), ValidReleaseOrigin, 5500.0f,
        FVector::ForwardVector, 0.0f);
    RejectsAtomically(TEXT("Infinite release origin"),
        FVector::ZeroVector, FVector(0.0f, Infinity, 0.0f), 5500.0f,
        FVector::ForwardVector, 0.0f);
    RejectsAtomically(TEXT("Zero aim distance"),
        FVector::ZeroVector, ValidReleaseOrigin, 0.0f,
        FVector::ForwardVector, 0.0f);
    RejectsAtomically(TEXT("Negative aim distance"),
        FVector::ZeroVector, ValidReleaseOrigin, -1.0f,
        FVector::ForwardVector, 0.0f);
    RejectsAtomically(TEXT("NaN aim distance"),
        FVector::ZeroVector, ValidReleaseOrigin, NaN,
        FVector::ForwardVector, 0.0f);
    RejectsAtomically(TEXT("Infinite aim distance"),
        FVector::ZeroVector, ValidReleaseOrigin, Infinity,
        FVector::ForwardVector, 0.0f);
    RejectsAtomically(TEXT("Zero base direction"),
        FVector::ZeroVector, ValidReleaseOrigin, 5500.0f,
        FVector::ZeroVector, 0.0f);
    RejectsAtomically(TEXT("Vertical base direction"),
        FVector::ZeroVector, ValidReleaseOrigin, 5500.0f,
        FVector::UpVector, 0.0f);
    RejectsAtomically(TEXT("NaN base direction"),
        FVector::ZeroVector, ValidReleaseOrigin, 5500.0f,
        FVector(NaN, 0.0f, 0.0f), 0.0f);
    RejectsAtomically(TEXT("Infinite base-direction Z"),
        FVector::ZeroVector, ValidReleaseOrigin, 5500.0f,
        FVector(1.0f, 0.0f, Infinity), 0.0f);
    RejectsAtomically(TEXT("NaN aim offset"),
        FVector::ZeroVector, ValidReleaseOrigin, 5500.0f,
        FVector::ForwardVector, NaN);
    RejectsAtomically(TEXT("Infinite aim offset"),
        FVector::ZeroVector, ValidReleaseOrigin, 5500.0f,
        FVector::ForwardVector, Infinity);
    RejectsAtomically(TEXT("Aim offset above the release contract"),
        FVector::ZeroVector, ValidReleaseOrigin, 5500.0f,
        FVector::ForwardVector,
        DiscGolfMath::ThrowReleaseMaximumAimOffsetDeg + 0.01f);
    RejectsAtomically(TEXT("Aim offset below the release contract"),
        FVector::ZeroVector, ValidReleaseOrigin, 5500.0f,
        FVector::ForwardVector,
        -DiscGolfMath::ThrowReleaseMaximumAimOffsetDeg - 0.01f);
    RejectsAtomically(TEXT("Derived displacement overflow"),
        FVector(std::numeric_limits<double>::max(), 0.0, 0.0),
        FVector(-std::numeric_limits<double>::max(), 0.0, 0.0),
        100.0f, FVector::ForwardVector, 0.0f);
    RejectsAtomically(TEXT("Grip coincident with the aim point"),
        FVector::ZeroVector, FVector(100.0f, 0.0f, 35.0f),
        100.0f, FVector::ForwardVector, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfThrowCommandContractValidTest,
    "DiscGolfTour.Release.CommandContract.Valid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfThrowCommandContractValidTest::RunTest(const FString& Parameters)
{
    const FThrowCommand Typical = MakeReleaseCommand(0.0f);
    TestTrue(TEXT("Typical controller command is accepted"), DiscGolfMath::IsThrowCommandValid(Typical));

    FThrowCommand LowerBounds = Typical;
    LowerBounds.Power01 = DiscGolfMath::ThrowCommandMinimumPower01;
    LowerBounds.HyzerDeg = DiscGolfMath::ThrowCommandMinimumHyzerDeg;
    LowerBounds.NoseAngleDeg = DiscGolfMath::ThrowCommandMinimumNoseAngleDeg;
    LowerBounds.LaunchAngleDeg = DiscGolfMath::ThrowCommandMinimumLaunchAngleDeg;
    LowerBounds.TimingError = DiscGolfMath::ThrowCommandMinimumTimingError;
    TestTrue(TEXT("Inclusive lower scalar bounds are accepted"),
        DiscGolfMath::IsThrowCommandValid(LowerBounds));

    FThrowCommand UpperBounds = Typical;
    UpperBounds.Power01 = DiscGolfMath::ThrowCommandMaximumPower01;
    UpperBounds.HyzerDeg = DiscGolfMath::ThrowCommandMaximumHyzerDeg;
    UpperBounds.NoseAngleDeg = DiscGolfMath::ThrowCommandMaximumNoseAngleDeg;
    UpperBounds.LaunchAngleDeg = DiscGolfMath::ThrowCommandMaximumLaunchAngleDeg;
    UpperBounds.TimingError = DiscGolfMath::ThrowCommandMaximumTimingError;
    TestTrue(TEXT("Inclusive upper scalar bounds are accepted"),
        DiscGolfMath::IsThrowCommandValid(UpperBounds));

    constexpr EDiscPlastic Plastics[] = {
        EDiscPlastic::Base, EDiscPlastic::Tour, EDiscPlastic::Crystal
    };
    for (const EDiscPlastic Plastic : Plastics)
    {
        FThrowCommand Command = Typical;
        Command.Plastic = Plastic;
        TestTrue(TEXT("Every authored plastic is accepted"), DiscGolfMath::IsThrowCommandValid(Command));
    }

    constexpr EThrowStyle ThrowStyles[] = { EThrowStyle::Backhand, EThrowStyle::Forehand };
    for (const EThrowStyle ThrowStyle : ThrowStyles)
    {
        FThrowCommand Command = Typical;
        Command.ThrowStyle = ThrowStyle;
        TestTrue(TEXT("Every authored throw style is accepted"), DiscGolfMath::IsThrowCommandValid(Command));
    }

    constexpr EDGHandedness HandednessValues[] = {
        EDGHandedness::Right, EDGHandedness::Left
    };
    for (const EDGHandedness Handedness : HandednessValues)
    {
        FThrowCommand Command = Typical;
        Command.Handedness = Handedness;
        TestTrue(TEXT("Every authored handedness is accepted"), DiscGolfMath::IsThrowCommandValid(Command));
    }

    constexpr EDiscShotContext ShotContexts[] = {
        EDiscShotContext::Drive, EDiscShotContext::Circle2Putt, EDiscShotContext::Circle1Putt
    };
    for (const EDiscShotContext ShotContext : ShotContexts)
    {
        FThrowCommand Command = Typical;
        Command.ShotContext = ShotContext;
        TestTrue(TEXT("Every authored shot context is accepted"), DiscGolfMath::IsThrowCommandValid(Command));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfThrowCommandContractRejectsInvalidTest,
    "DiscGolfTour.Release.CommandContract.RejectsInvalid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfThrowCommandContractRejectsInvalidTest::RunTest(const FString& Parameters)
{
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();

    FThrowCommand Invalid = MakeReleaseCommand(0.0f);
    Invalid.Direction = FVector::ZeroVector;
    TestFalse(TEXT("Zero direction is rejected"), DiscGolfMath::IsThrowCommandValid(Invalid));
    Invalid = MakeReleaseCommand(0.0f);
    Invalid.Direction.X = NaN;
    TestFalse(TEXT("NaN direction is rejected"), DiscGolfMath::IsThrowCommandValid(Invalid));
    Invalid = MakeReleaseCommand(0.0f);
    Invalid.Direction.Z = Infinity;
    TestFalse(TEXT("Infinite direction is rejected"), DiscGolfMath::IsThrowCommandValid(Invalid));
    Invalid = MakeReleaseCommand(0.0f);
    Invalid.Direction = FVector::UpVector;
    TestFalse(TEXT("Direction without a usable horizontal aim is rejected"),
        DiscGolfMath::IsThrowCommandValid(Invalid));

    Invalid = MakeReleaseCommand(0.0f);
    Invalid.MoldId = NAME_None;
    TestFalse(TEXT("Missing mold identity is rejected"), DiscGolfMath::IsThrowCommandValid(Invalid));
    Invalid = MakeReleaseCommand(0.0f);
    Invalid.Plastic = static_cast<EDiscPlastic>(255);
    TestFalse(TEXT("Unknown plastic is rejected"), DiscGolfMath::IsThrowCommandValid(Invalid));
    Invalid = MakeReleaseCommand(0.0f);
    Invalid.ThrowStyle = static_cast<EThrowStyle>(255);
    TestFalse(TEXT("Unknown throw style is rejected"), DiscGolfMath::IsThrowCommandValid(Invalid));
    Invalid = MakeReleaseCommand(0.0f);
    Invalid.Handedness = static_cast<EDGHandedness>(255);
    TestFalse(TEXT("Unknown handedness is rejected"), DiscGolfMath::IsThrowCommandValid(Invalid));
    Invalid = MakeReleaseCommand(0.0f);
    Invalid.ShotContext = static_cast<EDiscShotContext>(255);
    TestFalse(TEXT("Unknown shot context is rejected"), DiscGolfMath::IsThrowCommandValid(Invalid));

    const auto TestRejectedScalar = [this](
        const TCHAR* Label, float FThrowCommand::* Field, float Value)
    {
        FThrowCommand Command = MakeReleaseCommand(0.0f);
        Command.*Field = Value;
        TestFalse(Label, DiscGolfMath::IsThrowCommandValid(Command));
    };
    constexpr float Below = 0.001f;
    TestRejectedScalar(TEXT("NaN power is rejected"), &FThrowCommand::Power01, NaN);
    TestRejectedScalar(TEXT("Infinite power is rejected"), &FThrowCommand::Power01, Infinity);
    TestRejectedScalar(TEXT("Power below contract is rejected"), &FThrowCommand::Power01,
        DiscGolfMath::ThrowCommandMinimumPower01 - Below);
    TestRejectedScalar(TEXT("Power above contract is rejected"), &FThrowCommand::Power01,
        DiscGolfMath::ThrowCommandMaximumPower01 + Below);
    TestRejectedScalar(TEXT("NaN hyzer is rejected"), &FThrowCommand::HyzerDeg, NaN);
    TestRejectedScalar(TEXT("Infinite hyzer is rejected"), &FThrowCommand::HyzerDeg, Infinity);
    TestRejectedScalar(TEXT("Hyzer below contract is rejected"), &FThrowCommand::HyzerDeg,
        DiscGolfMath::ThrowCommandMinimumHyzerDeg - Below);
    TestRejectedScalar(TEXT("Hyzer above contract is rejected"), &FThrowCommand::HyzerDeg,
        DiscGolfMath::ThrowCommandMaximumHyzerDeg + Below);
    TestRejectedScalar(TEXT("NaN nose angle is rejected"), &FThrowCommand::NoseAngleDeg, NaN);
    TestRejectedScalar(TEXT("Infinite nose angle is rejected"), &FThrowCommand::NoseAngleDeg, Infinity);
    TestRejectedScalar(TEXT("Nose angle below contract is rejected"), &FThrowCommand::NoseAngleDeg,
        DiscGolfMath::ThrowCommandMinimumNoseAngleDeg - Below);
    TestRejectedScalar(TEXT("Nose angle above contract is rejected"), &FThrowCommand::NoseAngleDeg,
        DiscGolfMath::ThrowCommandMaximumNoseAngleDeg + Below);
    TestRejectedScalar(TEXT("NaN launch angle is rejected"), &FThrowCommand::LaunchAngleDeg, NaN);
    TestRejectedScalar(TEXT("Infinite launch angle is rejected"), &FThrowCommand::LaunchAngleDeg, Infinity);
    TestRejectedScalar(TEXT("Launch angle below contract is rejected"), &FThrowCommand::LaunchAngleDeg,
        DiscGolfMath::ThrowCommandMinimumLaunchAngleDeg - Below);
    TestRejectedScalar(TEXT("Launch angle above contract is rejected"), &FThrowCommand::LaunchAngleDeg,
        DiscGolfMath::ThrowCommandMaximumLaunchAngleDeg + Below);
    TestRejectedScalar(TEXT("NaN timing error is rejected"), &FThrowCommand::TimingError, NaN);
    TestRejectedScalar(TEXT("Infinite timing error is rejected"), &FThrowCommand::TimingError, Infinity);
    TestRejectedScalar(TEXT("Timing error below contract is rejected"), &FThrowCommand::TimingError,
        DiscGolfMath::ThrowCommandMinimumTimingError - Below);
    TestRejectedScalar(TEXT("Timing error above contract is rejected"), &FThrowCommand::TimingError,
        DiscGolfMath::ThrowCommandMaximumTimingError + Below);
    return true;
}

#endif
