#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DiscGolfEnvironmentController.h"
#include "DiscGolfPcgAuthoringController.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPcgShippingSeparationTest,
    "DiscGolfTour.Session19.PcgShippingSeparation.EditorAuthoringOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPcgShippingSeparationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const ADiscGolfEnvironmentController* RuntimeController =
        GetDefault<ADiscGolfEnvironmentController>();
    const ADiscGolfPcgAuthoringController* AuthoringController =
        GetDefault<ADiscGolfPcgAuthoringController>();

    TestNull(TEXT("Runtime controller has no reflected PCG component"),
        RuntimeController->GetClass()->FindPropertyByName(TEXT("PCGComponent")));
    TestNull(TEXT("Runtime controller exposes no forest generation function"),
        RuntimeController->GetClass()->FindFunctionByName(TEXT("GenerateForest")));
    TestNull(TEXT("Runtime controller exposes no forest cleanup function"),
        RuntimeController->GetClass()->FindFunctionByName(TEXT("CleanupForest")));
    TestNotNull(TEXT("Editor authoring controller exposes explicit forest generation"),
        AuthoringController->GetClass()->FindFunctionByName(TEXT("GenerateForest")));
    TestNotNull(TEXT("Editor authoring controller owns the PCG component"),
        AuthoringController->PCGComponent.Get());
    if (AuthoringController->PCGComponent)
    {
        TestEqual(TEXT("Authoring generation is explicit and on-demand"),
            static_cast<int32>(AuthoringController->PCGComponent->GenerationTrigger),
            static_cast<int32>(EPCGComponentGenerationTrigger::GenerateOnDemand));
        TestFalse(TEXT("Dropping the authoring actor never starts generation"),
            AuthoringController->PCGComponent->bGenerateOnDropWhenTriggerOnDemand);
    }
    TestFalse(TEXT("Neither controller ticks"),
        RuntimeController->PrimaryActorTick.bCanEverTick
        || AuthoringController->PrimaryActorTick.bCanEverTick);
    TestNotNull(TEXT("Preserved PCG source graph loads in the Editor target"),
        LoadObject<UPCGGraphInterface>(nullptr,
            TEXT("/Game/Environment/Forest/PCG/PCG_TemperateMountainForest.PCG_TemperateMountainForest")));
    return !HasAnyErrors();
}

#endif
