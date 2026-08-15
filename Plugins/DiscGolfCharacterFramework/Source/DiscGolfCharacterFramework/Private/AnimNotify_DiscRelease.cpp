#include "AnimNotify_DiscRelease.h"
#include "DiscGolfThrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_DiscRelease::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner())
    {
        return;
    }

    if (UDiscGolfThrowComponent* ThrowComponent = MeshComp->GetOwner()->FindComponentByClass<UDiscGolfThrowComponent>())
    {
        ThrowComponent->NotifyDiscRelease(MeshComp);
    }
}
