#include "AnimNotify_ThrowFinished.h"
#include "DiscGolfThrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_ThrowFinished::Notify(
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
        ThrowComponent->NotifyThrowFinished();
    }
}
