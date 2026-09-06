#include "DiscGolfPcgAuthoringController.h"

#include "DiscGolfEnvironmentDataAssets.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

ADiscGolfPcgAuthoringController::ADiscGolfPcgAuthoringController()
{
    PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("ForestPCGAuthoring"));
    PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    PCGComponent->bGenerateOnDropWhenTriggerOnDemand = false;
    PCGComponent->bIsComponentPartitioned = true;
    Tags.AddUnique(TEXT("Environment.Authoring.PCG"));
}

void ADiscGolfPcgAuthoringController::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyAuthoringPreset();
}

void ADiscGolfPcgAuthoringController::ApplyAuthoringPreset()
{
    UDiscGolfForestPreset* Preset = ForestPreset.LoadSynchronous();
    if (!Preset || !PCGComponent) return;

    PCGComponent->Seed = Preset->Clearance.RandomSeed;
#if WITH_EDITORONLY_DATA
    if (UPCGGraphInterface* Graph = Cast<UPCGGraphInterface>(
        Preset->ForestGraph.LoadSynchronous()))
    {
        PCGComponent->SetGraph(Graph);
    }
#endif
}

void ADiscGolfPcgAuthoringController::GenerateForest()
{
    ApplyAuthoringPreset();
    if (PCGComponent && PCGComponent->GetGraph())
    {
        PCGComponent->GenerateLocal(EPCGComponentGenerationTrigger::GenerateOnDemand, true,
            PCGHiGenGrid::UninitializedGridSize());
    }
}

void ADiscGolfPcgAuthoringController::CleanupForest()
{
    if (PCGComponent)
    {
        PCGComponent->CleanupLocal(true);
    }
}
