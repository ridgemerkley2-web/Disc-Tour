#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFixtureCollisionDynamicsTest,
    "DiscGolfTour.Fixtures.CollisionDynamics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFixtureCollisionDynamicsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FVector HeadOnVelocity(20.0f, 0.0f, 0.0f);
    const FVector WallNormal(-1.0f, 0.0f, 0.0f);
    const FDiscGolfFixtureImpactResult Tree = DiscGolfMath::ResolveFixtureImpact(
        HeadOnVelocity, WallNormal, EDiscGolfFixtureType::Tree);
    const FDiscGolfFixtureImpactResult Rock = DiscGolfMath::ResolveFixtureImpact(
        HeadOnVelocity, WallNormal, EDiscGolfFixtureType::Rock);
    const FDiscGolfFixtureImpactResult Sign = DiscGolfMath::ResolveFixtureImpact(
        HeadOnVelocity, WallNormal, EDiscGolfFixtureType::Sign);
    const FDiscGolfFixtureImpactResult Unknown = DiscGolfMath::ResolveFixtureImpact(
        HeadOnVelocity, WallNormal, EDiscGolfFixtureType::Unknown);

    TestTrue(TEXT("All solid fixtures reflect a head-on throw away from the surface"),
        Tree.VelocityMps.X < 0.0f && Rock.VelocityMps.X < 0.0f
        && Sign.VelocityMps.X < 0.0f && Unknown.VelocityMps.X < 0.0f);
    TestTrue(TEXT("Boulder rebound is stronger than sign, tree, and generic obstacle rebound"),
        Rock.VelocityMps.Size() > Sign.VelocityMps.Size()
        && Sign.VelocityMps.Size() > Tree.VelocityMps.Size()
        && Tree.VelocityMps.Size() > Unknown.VelocityMps.Size());
    TestTrue(TEXT("No solid fixture adds translational energy"),
        Rock.VelocityMps.Size() <= HeadOnVelocity.Size()
        && Sign.VelocityMps.Size() <= HeadOnVelocity.Size()
        && Tree.VelocityMps.Size() <= HeadOnVelocity.Size());
    TestTrue(TEXT("Tree contact sheds more spin than sign or rock"),
        Tree.SpinMultiplier < Sign.SpinMultiplier && Sign.SpinMultiplier < Rock.SpinMultiplier);

    const FVector GlancingVelocity(15.0f, 8.0f, 0.0f);
    const FDiscGolfFixtureImpactResult GlancingRock = DiscGolfMath::ResolveFixtureImpact(
        GlancingVelocity, WallNormal, EDiscGolfFixtureType::Rock);
    TestTrue(TEXT("Glancing rock contact retains a tangential component"), GlancingRock.VelocityMps.Y > 0.0f);
    TestTrue(TEXT("Glancing rock contact still loses total energy"),
        GlancingRock.VelocityMps.Size() < GlancingVelocity.Size());

    const FDiscGolfFixtureImpactResult DenseGrass = DiscGolfMath::ResolveFixtureOverlap(
        GlancingVelocity, EDiscGolfFixtureType::DenseGrass);
    TestTrue(TEXT("Dense grass is a pass-through overlap interaction"), DenseGrass.bPassThrough);
    TestTrue(TEXT("Dense grass preserves the incoming direction"),
        DenseGrass.VelocityMps.GetSafeNormal().Equals(GlancingVelocity.GetSafeNormal(), KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Dense grass retains exactly the authored speed fraction"),
        FMath::IsNearlyEqual(DenseGrass.VelocityMps.Size(), GlancingVelocity.Size() * 0.56f, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Dense grass sheds spin while passing through"),
        FMath::IsNearlyEqual(DenseGrass.SpinMultiplier, 0.64f, KINDA_SMALL_NUMBER));

    TestFalse(TEXT("Trees use blocking collision"),
        DiscGolfMath::FixtureImpactProfile(EDiscGolfFixtureType::Tree).bOverlapVolume);
    TestTrue(TEXT("Only dense grass uses the overlap profile"),
        DiscGolfMath::FixtureImpactProfile(EDiscGolfFixtureType::DenseGrass).bOverlapVolume);
    return true;
}

#endif
