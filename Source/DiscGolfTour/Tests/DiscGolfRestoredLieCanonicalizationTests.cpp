#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCourseRules.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfRestoredLieCanonicalizationTest,
    "DiscGolfTour.Rules.Restore.SupportedLieGroundCanonicalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfRestoredLieCanonicalizationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FVector Basket(58014.594883f, 8785.441130f, -600.0f);
    const FVector RawOutOfBoundsImpact(61200.0f, 9100.0f, -470.0f);
    const FVector LegacyPlayableLie(57949.647194f, 8662.072946f, -585.954895f);
    const FVector SupportedGround(57949.647194f, 8662.072946f, -598.954956f);
    const FDiscGolfLieState Saved = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::OutOfBounds,
        RawOutOfBoundsImpact,
        LegacyPlayableLie,
        Basket);

    FDiscGolfLieState Canonical;
    Canonical.SchemaVersion = 77;
    TestTrue(TEXT("A legacy playable lie projects onto supported course ground"),
        DiscGolfCourseRules::TryProjectRestoredLieToSupportedGround(
            Saved, true, SupportedGround, Canonical));
    TestTrue(TEXT("Ground projection preserves the playable lie XY"),
        FVector2D(Canonical.LieLocationCm).Equals(FVector2D(LegacyPlayableLie), 0.001f));
    TestTrue(TEXT("Ground projection removes the legacy collision clearance"),
        FMath::IsNearlyEqual(Canonical.LieLocationCm.Z, SupportedGround.Z, 0.001f));
    TestTrue(TEXT("The source snapshot remains immutable"),
        Saved.LieLocationCm.Equals(LegacyPlayableLie, 0.001f));

    TestTrue(TEXT("Raw impact provenance is preserved"),
        Canonical.RawDiscLocationCm.Equals(RawOutOfBoundsImpact, 0.001f));
    TestEqual(TEXT("Resting-surface provenance is preserved"),
        Canonical.SurfaceAtRest, Saved.SurfaceAtRest);
    TestEqual(TEXT("Playing-surface provenance is preserved"),
        Canonical.PlayingSurface, Saved.PlayingSurface);
    TestEqual(TEXT("Penalty provenance is preserved"),
        Canonical.PenaltyType, Saved.PenaltyType);
    TestEqual(TEXT("Relief provenance is preserved"),
        Canonical.ReliefRule, Saved.ReliefRule);
    TestEqual(TEXT("Penalty strokes are preserved"),
        Canonical.PenaltyStrokes, Saved.PenaltyStrokes);
    TestEqual(TEXT("Lie classification is preserved"),
        Canonical.LieType, Saved.LieType);
    TestEqual(TEXT("Shot context is preserved"),
        Canonical.ShotContext, Saved.ShotContext);
    TestTrue(TEXT("Planar basket distance is preserved"),
        FMath::IsNearlyEqual(
            Canonical.DistanceToBasketMeters,
            Saved.DistanceToBasketMeters,
            KINDA_SMALL_NUMBER));
    TestEqual(TEXT("Lie-effect identity is preserved"),
        Canonical.Effects.ProfileId, Saved.Effects.ProfileId);
    TestTrue(TEXT("Lie power provenance is preserved"),
        FMath::IsNearlyEqual(
            Canonical.Effects.PowerMultiplier,
            Saved.Effects.PowerMultiplier,
            KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Lie timing provenance is preserved"),
        FMath::IsNearlyEqual(
            Canonical.Effects.TimingErrorMultiplier,
            Saved.Effects.TimingErrorMultiplier,
            KINDA_SMALL_NUMBER));

    FDiscGolfLieState Rejected = Canonical;
    Rejected.SchemaVersion = 91;
    const FDiscGolfLieState RejectedSentinel = Rejected;
    TestFalse(TEXT("A missing surface trace cannot canonicalize a restored lie"),
        DiscGolfCourseRules::TryProjectRestoredLieToSupportedGround(
            Saved, false, SupportedGround, Rejected));
    TestEqual(TEXT("Missing-trace rejection leaves the caller output unchanged"),
        Rejected.SchemaVersion, RejectedSentinel.SchemaVersion);
    TestTrue(TEXT("Missing-trace rejection preserves the output location"),
        Rejected.LieLocationCm.Equals(RejectedSentinel.LieLocationCm));

    const FVector UnsupportedGround(
        LegacyPlayableLie.X,
        LegacyPlayableLie.Y,
        LegacyPlayableLie.Z - 101.0f);
    TestFalse(TEXT("A vertically unsupported surface cannot canonicalize a restored lie"),
        DiscGolfCourseRules::TryProjectRestoredLieToSupportedGround(
            Saved, true, UnsupportedGround, Rejected));
    TestEqual(TEXT("Unsupported-ground rejection leaves the caller output unchanged"),
        Rejected.SchemaVersion, RejectedSentinel.SchemaVersion);
    TestTrue(TEXT("Unsupported-ground rejection preserves the output location"),
        Rejected.LieLocationCm.Equals(RejectedSentinel.LieLocationCm));

    const FVector InvalidGround(
        LegacyPlayableLie.X,
        LegacyPlayableLie.Y,
        std::numeric_limits<float>::quiet_NaN());
    TestFalse(TEXT("A non-finite surface cannot canonicalize a restored lie"),
        DiscGolfCourseRules::TryProjectRestoredLieToSupportedGround(
            Saved, true, InvalidGround, Rejected));
    TestEqual(TEXT("Non-finite rejection leaves the caller output unchanged"),
        Rejected.SchemaVersion, RejectedSentinel.SchemaVersion);
    TestTrue(TEXT("Non-finite rejection preserves the output location"),
        Rejected.LieLocationCm.Equals(RejectedSentinel.LieLocationCm));

    return true;
}

#endif
