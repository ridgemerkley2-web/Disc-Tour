#include "AnimNotify_DiscRelease.h"
#include "AnimNotify_ThrowFinished.h"
#include "AnimNotify_ThrowPhase.h"

#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimMontage.h"
#include "DiscGolfThrowComponent.h"
#include "GameFramework/Actor.h"

namespace
{
UDiscGolfThrowComponent* FindThrowComponent(USkeletalMeshComponent* MeshComp)
{
    return IsValid(MeshComp) && IsValid(MeshComp->GetOwner())
        ? MeshComp->GetOwner()->FindComponentByClass<UDiscGolfThrowComponent>()
        : nullptr;
}

int32 GetMontageInstanceId(const FAnimNotifyEventReference& EventReference)
{
    const UE::Anim::FAnimNotifyMontageInstanceContext* Context =
        EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
    return Context ? Context->MontageInstanceID : INDEX_NONE;
}

const UAnimMontage* GetSourceMontage(UAnimSequenceBase* Animation)
{
    return Cast<UAnimMontage>(Animation);
}
}

void UAnimNotify_ThrowPhase::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    if (UDiscGolfThrowComponent* Throw = FindThrowComponent(MeshComp))
    {
        Throw->NotifyThrowPhaseFromMontage(
            Phase,
            GetMontageInstanceId(EventReference),
            GetSourceMontage(Animation));
    }
}

void UAnimNotify_DiscRelease::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    if (UDiscGolfThrowComponent* Throw = FindThrowComponent(MeshComp))
    {
        Throw->NotifyDiscReleaseFromMontage(
            MeshComp,
            GetMontageInstanceId(EventReference),
            GetSourceMontage(Animation));
    }
}

void UAnimNotify_DiscRelease::BranchingPointNotify(
    FBranchingPointNotifyPayload& BranchingPointPayload)
{
    // UAnimNotify's default branching-point bridge creates an empty event
    // reference, so explicitly preserve the montage-instance identity carried
    // by the branching-point payload before forwarding release authority.
    Super::BranchingPointNotify(BranchingPointPayload);
    if (UDiscGolfThrowComponent* Throw =
            FindThrowComponent(BranchingPointPayload.SkelMeshComponent))
    {
        Throw->NotifyDiscReleaseFromMontage(
            BranchingPointPayload.SkelMeshComponent,
            BranchingPointPayload.MontageInstanceID,
            Cast<UAnimMontage>(BranchingPointPayload.SequenceAsset));
    }
}

void UAnimNotify_ThrowFinished::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    if (UDiscGolfThrowComponent* Throw = FindThrowComponent(MeshComp))
    {
        Throw->NotifyThrowFinishedFromMontage(
            GetMontageInstanceId(EventReference),
            GetSourceMontage(Animation));
    }
}

void UAnimNotify_ThrowFinished::BranchingPointNotify(
    FBranchingPointNotifyPayload& BranchingPointPayload)
{
    Super::BranchingPointNotify(BranchingPointPayload);
    if (UDiscGolfThrowComponent* Throw =
            FindThrowComponent(BranchingPointPayload.SkelMeshComponent))
    {
        Throw->NotifyThrowFinishedFromMontage(
            BranchingPointPayload.MontageInstanceID,
            Cast<UAnimMontage>(BranchingPointPayload.SequenceAsset));
    }
}
