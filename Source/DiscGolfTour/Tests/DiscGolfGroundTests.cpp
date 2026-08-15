#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGroundSurfaceProfilesTest,
    "DiscGolfTour.Ground.SurfaceProfiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGroundSurfaceProfilesTest::RunTest(const FString& Parameters)
{
    const FGroundSurfaceProfile Fairway = DiscGolfMath::GroundSurfaceProfile(EGroundSurfaceType::Fairway);
    const FGroundSurfaceProfile Rough = DiscGolfMath::GroundSurfaceProfile(EGroundSurfaceType::Rough);
    const FGroundSurfaceProfile Rock = DiscGolfMath::GroundSurfaceProfile(EGroundSurfaceType::Rock);
    const FGroundSurfaceProfile Tee = DiscGolfMath::GroundSurfaceProfile(EGroundSurfaceType::TeePad);

    TestTrue(TEXT("Rough has the strongest slide deceleration"),
        Rough.SlideDecelerationMps2 > Fairway.SlideDecelerationMps2 &&
        Fairway.SlideDecelerationMps2 > Rock.SlideDecelerationMps2);
    TestTrue(TEXT("Rock amplifies restitution"), Rock.RestitutionScale > Fairway.RestitutionScale);
    TestTrue(TEXT("Rough absorbs spin"), Rough.ImpactSpinRetention < Fairway.ImpactSpinRetention);
    TestTrue(TEXT("Tee pad is slicker than fairway"), Tee.FrictionScale < Fairway.FrictionScale);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGroundSkipClassificationTest,
    "DiscGolfTour.Ground.Classification.ShallowSkip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGroundSkipClassificationTest::RunTest(const FString& Parameters)
{
    const FVector Velocity(10.0f, 0.0f, -1.5f);
    const FGroundImpactResult Fairway = DiscGolfMath::ResolveGroundImpact(
        Velocity, FVector::UpVector, FVector::UpVector, 700.0f, 0.16f, 0.46f, EGroundSurfaceType::Fairway);
    const FGroundImpactResult Rough = DiscGolfMath::ResolveGroundImpact(
        Velocity, FVector::UpVector, FVector::UpVector, 700.0f, 0.16f, 0.46f, EGroundSurfaceType::Rough);
    const FGroundSurfaceProfile FairwayProfile = DiscGolfMath::GroundSurfaceProfile(EGroundSurfaceType::Fairway);
    const FGroundImpactResult AfterSkipBudget = DiscGolfMath::ResolveGroundImpact(
        Velocity, FVector::UpVector, FVector::UpVector, 700.0f, 0.16f, 0.46f,
        EGroundSurfaceType::Fairway, FairwayProfile.MaxConsecutiveSkips);

    TestEqual(TEXT("Shallow fast fairway impact skips"), Fairway.State, EDiscGroundState::Skipping);
    TestEqual(TEXT("The same impact sticks into a rough slide"), Rough.State, EDiscGroundState::Sliding);
    TestTrue(TEXT("Skip leaves the surface"), Fairway.VelocityMps.Z > 0.0f);
    TestTrue(TEXT("Skip preserves forward direction"), Fairway.VelocityMps.X > 0.0f);
    TestEqual(TEXT("A fairway skip train transitions to a slide at its surface budget"),
        AfterSkipBudget.State, EDiscGroundState::Sliding);
    TestTrue(TEXT("The skip-out slide absorbs the failed rebound"),
        AfterSkipBudget.VelocityMps.Size() < Velocity.Size() * 0.70f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGroundEdgeRollClassificationTest,
    "DiscGolfTour.Ground.Classification.EdgeRoll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGroundEdgeRollClassificationTest::RunTest(const FString& Parameters)
{
    const FGroundImpactResult Result = DiscGolfMath::ResolveGroundImpact(
        FVector(7.0f, 0.0f, -2.0f), FVector::UpVector, FVector::RightVector,
        500.0f, 0.16f, 0.46f, EGroundSurfaceType::Fairway);

    TestEqual(TEXT("Fast edge-on impact starts an edge roll"), Result.State, EDiscGroundState::EdgeRolling);
    TestTrue(TEXT("Edge angle is reported"), Result.DiscEdgeAngleDeg > 89.0f);
    TestTrue(TEXT("Edge roll is constrained to the surface"), FMath::IsNearlyZero(Result.VelocityMps.Z));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGroundSlideAndSettleClassificationTest,
    "DiscGolfTour.Ground.Classification.SlideAndSettle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGroundSlideAndSettleClassificationTest::RunTest(const FString& Parameters)
{
    const FGroundImpactResult Steep = DiscGolfMath::ResolveGroundImpact(
        FVector(8.0f, 0.0f, -8.0f), FVector::UpVector, FVector::UpVector,
        600.0f, 0.16f, 0.46f, EGroundSurfaceType::Fairway);
    const FGroundImpactResult Slow = DiscGolfMath::ResolveGroundImpact(
        FVector(0.6f, 0.0f, -0.4f), FVector::UpVector, FVector::UpVector,
        80.0f, 0.16f, 0.46f, EGroundSurfaceType::Fairway);

    TestEqual(TEXT("Steep non-edge impact slides"), Steep.State, EDiscGroundState::Sliding);
    TestEqual(TEXT("Low-energy impact settles"), Slow.State, EDiscGroundState::Settled);
    TestTrue(TEXT("Settled velocity is zero"), Slow.VelocityMps.IsNearlyZero());
    TestTrue(TEXT("Settled impact removes spin"), FMath::IsNearlyZero(Slow.SpinMultiplier));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGroundPlasticResponseTest,
    "DiscGolfTour.Ground.PlasticChangesSkipThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGroundPlasticResponseTest::RunTest(const FString& Parameters)
{
    const FVector BorderlineSkip(7.4f, 0.0f, -1.0f);
    const FGroundImpactResult BasePlastic = DiscGolfMath::ResolveGroundImpact(
        BorderlineSkip, FVector::UpVector, FVector::UpVector, 650.0f,
        0.16f * 0.72f, 0.46f * 1.18f, EGroundSurfaceType::Fairway);
    const FGroundImpactResult CrystalPlastic = DiscGolfMath::ResolveGroundImpact(
        BorderlineSkip, FVector::UpVector, FVector::UpVector, 650.0f,
        0.16f * 1.20f, 0.46f * 0.86f, EGroundSurfaceType::Fairway);

    TestEqual(TEXT("Grippy base plastic turns the borderline skip into a slide"), BasePlastic.State, EDiscGroundState::Sliding);
    TestEqual(TEXT("Stiff crystal plastic clears the borderline skip threshold"), CrystalPlastic.State, EDiscGroundState::Skipping);
    TestTrue(TEXT("Crystal retains more forward speed"), CrystalPlastic.VelocityMps.X > BasePlastic.VelocityMps.X);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGroundHandednessTest,
    "DiscGolfTour.Ground.SpinSignDoesNotForkPhysics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGroundHandednessTest::RunTest(const FString& Parameters)
{
    const FVector Velocity(10.0f, 0.0f, -1.5f);
    const FGroundImpactResult PositiveSpin = DiscGolfMath::ResolveGroundImpact(
        Velocity, FVector::UpVector, FVector::UpVector, 700.0f, 0.16f, 0.46f, EGroundSurfaceType::Fairway);
    const FGroundImpactResult NegativeSpin = DiscGolfMath::ResolveGroundImpact(
        Velocity, FVector::UpVector, FVector::UpVector, -700.0f, 0.16f, 0.46f, EGroundSurfaceType::Fairway);

    TestEqual(TEXT("Spin sign shares impact classification"), PositiveSpin.State, NegativeSpin.State);
    TestTrue(TEXT("Spin sign shares translational response"), PositiveSpin.VelocityMps.Equals(NegativeSpin.VelocityMps));
    TestTrue(TEXT("Spin sign shares retention magnitude"), FMath::IsNearlyEqual(PositiveSpin.SpinMultiplier, NegativeSpin.SpinMultiplier));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGroundDecelerationTest,
    "DiscGolfTour.Ground.Deceleration.FixedStepStable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGroundDecelerationTest::RunTest(const FString& Parameters)
{
    const FVector Initial(10.0f, 0.0f, 0.0f);
    const FVector OneStep = DiscGolfMath::ApplyGroundDeceleration(Initial, 2.2f, 1.0f);
    FVector FixedSteps = Initial;
    constexpr float Dt = 1.0f / 240.0f;
    for (int32 Index = 0; Index < 240; ++Index)
    {
        FixedSteps = DiscGolfMath::ApplyGroundDeceleration(FixedSteps, 2.2f, Dt);
    }

    TestTrue(TEXT("One second removes the authored speed"), FMath::IsNearlyEqual(OneStep.Size(), 7.8f, 0.001f));
    TestTrue(TEXT("Fixed steps match one equivalent step"), FMath::IsNearlyEqual(FixedSteps.Size(), OneStep.Size(), 0.002f));
    TestTrue(TEXT("Deceleration cannot reverse velocity"),
        DiscGolfMath::ApplyGroundDeceleration(FVector(1.0f, 0.0f, 0.0f), 5.0f, 1.0f).IsNearlyZero());
    return true;
}

#endif
