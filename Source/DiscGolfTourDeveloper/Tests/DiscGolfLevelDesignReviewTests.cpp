#if WITH_DEV_AUTOMATION_TESTS

#include "DiscGolfLevelDesignReviewActor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfLevelDesignReviewCollisionInvariantTest,
    "DiscGolfTour.Developer.LevelDesignReview.CollisionInvariant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfLevelDesignReviewCollisionInvariantTest::RunTest(const FString& Parameters)
{
    const ADiscGolfLevelDesignReviewActor* Defaults =
        GetDefault<ADiscGolfLevelDesignReviewActor>();
    TestTrue(TEXT("Level-design review can never change competitive collision"),
        Defaults && Defaults->IsCollisionInvariant());
    return true;
}

#endif
