#include "DiscGolfVegetationInteractionActor.h"

#include "Components/BoxComponent.h"

ADiscGolfVegetationInteractionActor::ADiscGolfVegetationInteractionActor()
{
    PrimaryActorTick.bCanEverTick = false;
    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("VegetationInteraction"));
    SetRootComponent(InteractionVolume);
    InteractionVolume->SetCollisionObjectType(ECC_WorldStatic);
    InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionVolume->SetGenerateOverlapEvents(true);
    InteractionVolume->SetCanEverAffectNavigation(false);
    InteractionVolume->SetHiddenInGame(true);
    Tags.AddUnique(TEXT("Environment.VegetationInteraction"));
}

void ADiscGolfVegetationInteractionActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    InteractionVolume->SetBoxExtent(BoxExtentCm.GetAbs());
}

bool ADiscGolfVegetationInteractionActor::HasValidInteractionContract() const
{
    return InteractionVolume
        && InteractionVolume->GetCollisionEnabled() == ECollisionEnabled::QueryOnly
        && InteractionVolume->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Overlap
        && InteractionVolume->GetGenerateOverlapEvents()
        && Profile.VelocityMultiplier > 0.0f && Profile.VelocityMultiplier <= 1.0f
        && Profile.SpinMultiplier > 0.0f && Profile.SpinMultiplier <= 1.0f
        && FMath::IsFinite(Profile.ReentryCooldownSeconds)
        && Profile.ReentryCooldownSeconds >= 0.0f
        && Profile.ReentryCooldownSeconds <= 2.0f;
}
