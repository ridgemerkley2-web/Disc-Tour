#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCoursePresentationDefinition.h"
#include "../DiscGolfEnvironmentController.h"
#include "../DiscGolfEnvironmentDataAssets.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession10EnvironmentFailClosedTest,
    "DiscGolfTour.Session10.Environment.ContractFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession10EnvironmentFailClosedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const UDiscGolfEnvironmentAssetSet* DefaultAssets =
        GetDefault<UDiscGolfEnvironmentAssetSet>();
    TestEqual(TEXT("The environment schema retains exactly 16 binding slots"),
        DefaultAssets->Slots.Num(), 16);
    TestEqual(TEXT("Default environment bindings remain unpopulated"),
        DefaultAssets->GetPopulatedSlotCount(), 0);

    FDiscGolfCoursePresentationDefinition Presentation;
    FString Source;
    FString Error;
    if (!TestTrue(TEXT("Pine Ridge presentation resolves"),
        DiscGolfCoursePresentation::LoadPineRidge(Presentation, Source, Error)))
    {
        AddError(Error);
        return false;
    }
    TestFalse(TEXT("Fallback presentation cannot claim production assets are ready"),
        Presentation.bAssetsReady);
    TestTrue(TEXT("Fallback presentation preserves competitive collision"),
        Presentation.bCollisionInvariantAcrossQuality);
    TestEqual(TEXT("Session 10 presentation covers exactly three holes"),
        Presentation.Holes.Num(), 3);
    if (Presentation.Holes.Num() == 3)
    {
        TestEqual(TEXT("Hole 1 uses the neutral forest reference"),
            Presentation.Holes[0].ForestReferenceId, FName(TEXT("OpeningBroadTreeLine")));
        TestEqual(TEXT("Hole 2 uses the neutral forest reference"),
            Presentation.Holes[1].ForestReferenceId, FName(TEXT("NeedleCanopyCompression")));
        TestEqual(TEXT("Hole 3 uses the neutral forest reference"),
            Presentation.Holes[2].ForestReferenceId, FName(TEXT("GalleryLakeFrame")));
    }

    const ADiscGolfEnvironmentController* DefaultController =
        GetDefault<ADiscGolfEnvironmentController>();
    TestFalse(TEXT("Environment-to-flight wind synchronization is opt-in and dormant by default"),
        DefaultController->bSynchronizeDiscFlightWind);
    TestFalse(TEXT("Default controller cannot claim a production configuration"),
        DefaultController->HasProductionConfiguration());
    TestFalse(TEXT("An empty configuration cannot be production-ready"),
        ADiscGolfEnvironmentController::IsProductionConfigurationComplete(16, 0, true));
    TestFalse(TEXT("Three of sixteen bindings cannot be production-ready"),
        ADiscGolfEnvironmentController::IsProductionConfigurationComplete(16, 3, true));
    TestFalse(TEXT("Complete bindings without an authoring graph cannot be production-ready"),
        ADiscGolfEnvironmentController::IsProductionConfigurationComplete(16, 16, false));
    TestTrue(TEXT("Authoring readiness requires exactly sixteen populated bindings and a graph"),
        ADiscGolfEnvironmentController::IsProductionConfigurationComplete(16, 16, true));
    TestFalse(TEXT("Environment controller never ticks"),
        DefaultController->PrimaryActorTick.bCanEverTick);
    TestNull(TEXT("Runtime controller exposes no serialized PCG component"),
        DefaultController->GetClass()->FindPropertyByName(TEXT("PCGComponent")));
    return !HasAnyErrors();
}

#endif
