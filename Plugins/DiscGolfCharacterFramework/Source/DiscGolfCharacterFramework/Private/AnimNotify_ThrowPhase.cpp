#include "AnimNotify_ThrowPhase.h"
#include "DiscGolfThrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_ThrowPhase::Notify(
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
        ThrowComponent->NotifyThrowPhase(Phase);
    }
}

#if WITH_EDITOR
FString UAnimNotify_ThrowPhase::GetNotifyName_Implementation() const
{
    if (const UEnum* Enum = StaticEnum<EDGThrowPhase>())
    {
        return FString::Printf(TEXT("DG Phase: %s"), *Enum->GetNameStringByValue(static_cast<int64>(Phase)));
    }
    return TEXT("DG Throw Phase");
}
#endif
