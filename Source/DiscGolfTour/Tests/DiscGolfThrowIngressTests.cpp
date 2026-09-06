#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "../DiscGolfTourGameMode.h"
#include "DiscGolfDiscTypes.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPlayerThrowProvenanceTest,
    "DiscGolfTour.Gameplay.ThrowIngress.Provenance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPlayerThrowProvenanceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDGDiscInstance Selected;
    Selected.InstanceId = FGuid::NewGuid();
    Selected.DiscDefinitionId = TEXT("Apex");
    Selected.PlasticId = TEXT("Tour");

    FThrowCommand Command;
    Command.DiscInstanceId = Selected.InstanceId;
    Command.MoldId = Selected.DiscDefinitionId;
    Command.Plastic = EDiscPlastic::Tour;
    Command.Handedness = EDGHandedness::Right;
    Command.ShotContext = EDiscShotContext::Drive;

    const auto Accepts = [&Selected](const FThrowCommand& Candidate,
        EDGHandedness ActiveHand = EDGHandedness::Right,
        EDiscShotContext CurrentContext = EDiscShotContext::Drive)
    {
        return ADiscGolfTourGameMode::IsPlayerThrowProvenanceValid(
            Candidate, ActiveHand, CurrentContext, Selected);
    };

    TestTrue(TEXT("Exact stable equipment, profile hand, and lie context are accepted"),
        Accepts(Command));

    FThrowCommand Mutated = Command;
    Mutated.DiscInstanceId.Invalidate();
    TestFalse(TEXT("Missing stable instance identity is rejected"), Accepts(Mutated));

    Mutated = Command;
    Mutated.DiscInstanceId = FGuid::NewGuid();
    TestFalse(TEXT("Different selected instance identity is rejected"), Accepts(Mutated));

    Mutated = Command;
    Mutated.MoldId = TEXT("Touch");
    TestFalse(TEXT("Mold provenance mismatch is rejected"), Accepts(Mutated));

    Mutated = Command;
    Mutated.Plastic = EDiscPlastic::Base;
    TestFalse(TEXT("Plastic provenance mismatch is rejected"), Accepts(Mutated));

    Mutated = Command;
    Mutated.Plastic = static_cast<EDiscPlastic>(255);
    TestFalse(TEXT("Unknown plastic provenance is rejected"), Accepts(Mutated));

    TestFalse(TEXT("Character-profile handedness mismatch is rejected"),
        Accepts(Command, EDGHandedness::Left));
    TestFalse(TEXT("Authoritative lie context mismatch is rejected"),
        Accepts(Command, EDGHandedness::Right, EDiscShotContext::Circle1Putt));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReleaseOriginPreflightIntegrationTest,
    "DiscGolfTour.Gameplay.ThrowIngress.ReleaseOriginPreflight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReleaseOriginPreflightIntegrationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FVector PreAnimationAimOriginCm(50000000.0, -35000000.0, 1200.0);
    const FVector AnimatedGripOriginCm = PreAnimationAimOriginCm
        + FVector(4.345f, 70.069f, 33.687f);
    FThrowRelease BaseRelease;
    BaseRelease.Direction = FVector::ForwardVector;
    BaseRelease.AimOffsetDeg = 0.0f;

    FThrowRelease NullPathRelease = BaseRelease;
    FString Error = TEXT("prepopulated error");
    const FVector InvalidAimOrigin(
        std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0);
    TestTrue(TEXT("Null override preserves the synchronous launch path"),
        ADiscGolfTourGameMode::TryPrepareReleaseForOriginOverride(
            nullptr, InvalidAimOrigin, -1.0f, NullPathRelease, Error));
    TestTrue(TEXT("Null override leaves the release direction unchanged"),
        NullPathRelease.Direction.Equals(BaseRelease.Direction, 0.0f));
    TestTrue(TEXT("Null override clears a prepopulated error"), Error.IsEmpty());

    FThrowRelease AnimatedRelease = BaseRelease;
    Error = TEXT("prepopulated error");
    TestTrue(TEXT("Valid non-null animated grip rebases the release"),
        ADiscGolfTourGameMode::TryPrepareReleaseForOriginOverride(
            &AnimatedGripOriginCm,
            PreAnimationAimOriginCm,
            5500.0f,
            AnimatedRelease,
            Error));
    TestFalse(TEXT("Non-null animated grip mutates the release direction"),
        AnimatedRelease.Direction.Equals(BaseRelease.Direction, 1.0e-6f));
    const double AnimatedYawDeg = FMath::RadiansToDegrees(FMath::Atan2(
        AnimatedRelease.Direction.Y, AnimatedRelease.Direction.X));
    TestTrue(TEXT("GameMode preflight applies the measured drive correction"),
        FMath::IsNearlyEqual(AnimatedYawDeg, -0.730475, 0.001));
    TestTrue(TEXT("Successful non-null preflight clears its error"), Error.IsEmpty());

    const FVector OutOfRangeGripOriginCm = PreAnimationAimOriginCm
        + FVector(
            ADiscGolfTourGameMode::MaximumAnimatedGripOriginDistanceCm + 0.01f,
            0.0f,
            0.0f);
    FThrowRelease OutOfRangeRelease = BaseRelease;
    Error.Reset();
    TestFalse(TEXT("Grip outside the authoritative pre-animation origin bound is rejected"),
        ADiscGolfTourGameMode::TryPrepareReleaseForOriginOverride(
            &OutOfRangeGripOriginCm,
            PreAnimationAimOriginCm,
            5500.0f,
            OutOfRangeRelease,
            Error));
    TestTrue(TEXT("Origin-bound failure leaves the release unchanged before spawn"),
        OutOfRangeRelease.Direction.Equals(BaseRelease.Direction, 0.0f));
    TestFalse(TEXT("Origin-bound failure reports an error"), Error.IsEmpty());

    FThrowRelease ShortRangeRelease = BaseRelease;
    Error.Reset();
    constexpr float TapInAimRangeCm = 250.0f;
    TestTrue(TEXT("A positive tap-in aim range inside the grip-origin bound is accepted"),
        ADiscGolfTourGameMode::TryPrepareReleaseForOriginOverride(
            &AnimatedGripOriginCm,
            PreAnimationAimOriginCm,
            TapInAimRangeCm,
            ShortRangeRelease,
            Error));
    const FVector TapInAimPointCm = PreAnimationAimOriginCm
        + FVector::ForwardVector * TapInAimRangeCm;
    const FVector ExpectedTapInDirection = FVector(
        TapInAimPointCm.X - AnimatedGripOriginCm.X,
        TapInAimPointCm.Y - AnimatedGripOriginCm.Y,
        0.0f).GetSafeNormal();
    TestTrue(TEXT("Tap-in release direction preserves the basket aim point"),
        ShortRangeRelease.Direction.Equals(ExpectedTapInDirection, 1.0e-5f));
    TestTrue(TEXT("Successful tap-in preflight clears its error"), Error.IsEmpty());

    FThrowRelease ZeroRangeRelease = BaseRelease;
    Error.Reset();
    TestFalse(TEXT("A zero animated aim range is rejected"),
        ADiscGolfTourGameMode::TryPrepareReleaseForOriginOverride(
            &AnimatedGripOriginCm,
            PreAnimationAimOriginCm,
            0.0f,
            ZeroRangeRelease,
            Error));
    TestTrue(TEXT("Zero-range failure leaves the release unchanged before spawn"),
        ZeroRangeRelease.Direction.Equals(BaseRelease.Direction, 0.0f));
    TestFalse(TEXT("Zero-range failure reports an error"), Error.IsEmpty());

    FThrowRelease InvalidDirectionRelease = BaseRelease;
    InvalidDirectionRelease.Direction = FVector::ZeroVector;
    Error.Reset();
    TestFalse(TEXT("Math-level rebase failure propagates through GameMode preflight"),
        ADiscGolfTourGameMode::TryPrepareReleaseForOriginOverride(
            &AnimatedGripOriginCm,
            PreAnimationAimOriginCm,
            5500.0f,
            InvalidDirectionRelease,
            Error));
    TestTrue(TEXT("Math-level failure preserves the invalid release for caller rejection"),
        InvalidDirectionRelease.Direction.Equals(FVector::ZeroVector, 0.0f));
    TestFalse(TEXT("Math-level failure reports an error"), Error.IsEmpty());
    return true;
}

#endif
