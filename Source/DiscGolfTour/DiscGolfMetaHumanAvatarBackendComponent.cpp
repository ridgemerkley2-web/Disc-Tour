#include "DiscGolfMetaHumanAvatarBackendComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

#include "DiscGolfAvatarBackendProfile.h"

#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfMetaHumanVisualContract.h"

namespace
{
USkeletalMeshComponent* Session8FindUniqueTaggedSkeletalMesh(
    AActor* VisualActor,
    FName RequiredTag,
    bool& bOutDuplicate)
{
    bOutDuplicate = false;
    USkeletalMeshComponent* Result = nullptr;
    TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
    VisualActor->GetComponents(SkeletalMeshes);
    for (USkeletalMeshComponent* Candidate : SkeletalMeshes)
    {
        if (!IsValid(Candidate) || !Candidate->ComponentHasTag(RequiredTag))
        {
            continue;
        }
        if (Result)
        {
            bOutDuplicate = true;
            return nullptr;
        }
        Result = Candidate;
    }
    return Result;
}

void Session8EnforceTaggedMeshSafety(USkeletalMeshComponent* Mesh)
{
    if (!IsValid(Mesh))
    {
        return;
    }
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCanEverAffectNavigation(false);
}
}

USkeletalMeshComponent*
UDiscGolfMetaHumanAvatarBackendComponent::GetVerifiedVisualBody() const
{
    return IsVisualBackendReady()
        && GetActiveVisualActor() == VerifiedVisualActor
        && IsValid(VerifiedVisualBody)
        ? VerifiedVisualBody.Get() : nullptr;
}

USkeletalMeshComponent*
UDiscGolfMetaHumanAvatarBackendComponent::GetVerifiedVisualHead() const
{
    return IsVisualBackendReady()
        && GetActiveVisualActor() == VerifiedVisualActor
        && IsValid(VerifiedVisualHead)
        ? VerifiedVisualHead.Get() : nullptr;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::ConfigureVisualBackend_Implementation(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    const FDGFullCharacterCustomization& Customization)
{
    UDiscGolfAvatarBackendProfile* CandidateProfile = BackendProfile.Get();
    FString ProfileStatus;
    if (!DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
            CandidateProfile, ProfileStatus))
    {
        LastAdapterStatus = ProfileStatus;
        return false;
    }
    if (!IsValid(VisualActor) || !IsValid(AnimationSourceMesh)
        || !VisualActor->GetClass()->ImplementsInterface(
            UDiscGolfMetaHumanVisualContract::StaticClass()))
    {
        LastAdapterStatus =
            TEXT("Assembled visual does not implement the verified project contract.");
        return false;
    }
    if (!CandidateProfile->RetargetAsset.LoadSynchronous())
    {
        LastAdapterStatus = TEXT("DG-to-MetaHuman retarget asset did not load.");
        return false;
    }

    FString ContractStatus;
    if (!IDiscGolfMetaHumanVisualContract::Execute_ConfigureFromDGAnimationSource(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            Customization,
            ContractStatus))
    {
        LastAdapterStatus = ContractStatus.IsEmpty()
            ? TEXT("Assembled visual rejected DG-source configuration.")
            : ContractStatus;
        return false;
    }

    USkeletalMeshComponent* CandidateBody = nullptr;
    USkeletalMeshComponent* CandidateHead = nullptr;
    FString ValidationStatus;
    if (!ValidateConfiguredVisual(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            CandidateBody,
            CandidateHead,
            ValidationStatus))
    {
        LastAdapterStatus = ValidationStatus;
        return false;
    }

    VerifiedVisualActor = VisualActor;
    VerifiedVisualBody = CandidateBody;
    VerifiedVisualHead = CandidateHead;
    LastAdapterStatus = TEXT("Assembled visual body/head and DG retarget verified.");
    return true;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::ApplyVisualCustomization_Implementation(
    AActor* VisualActor,
    const FDGFullCharacterCustomization& Customization)
{
    UDiscGolfAvatarBackendProfile* ActiveProfile = GetActiveBackendProfile();
    if (!IsValid(VisualActor)
        || VisualActor != VerifiedVisualActor
        || !IsValid(ActiveProfile)
        || !VisualActor->GetClass()->ImplementsInterface(
            UDiscGolfMetaHumanVisualContract::StaticClass()))
    {
        LastAdapterStatus = TEXT("No verified assembled visual is active.");
        return false;
    }

    FString ContractStatus;
    if (!IDiscGolfMetaHumanVisualContract::Execute_ApplyMappedCustomization(
            VisualActor,
            ActiveProfile,
            Customization,
            ContractStatus))
    {
        LastAdapterStatus = ContractStatus.IsEmpty()
            ? TEXT("Assembled visual rejected mapped customization.")
            : ContractStatus;
        return false;
    }

    USkeletalMeshComponent* CandidateBody = nullptr;
    USkeletalMeshComponent* CandidateHead = nullptr;
    FString ValidationStatus;
    if (!ValidateConfiguredVisual(
            VisualActor,
            GetActiveAnimationSourceMesh(),
            ActiveProfile,
            CandidateBody,
            CandidateHead,
            ValidationStatus))
    {
        LastAdapterStatus = ValidationStatus;
        return false;
    }

    VerifiedVisualBody = CandidateBody;
    VerifiedVisualHead = CandidateHead;
    LastAdapterStatus = TEXT("Mapped assembled-visual customization verified.");
    return true;
}

void UDiscGolfMetaHumanAvatarBackendComponent::OnUnregister()
{
    Super::OnUnregister();
    VerifiedVisualActor = nullptr;
    VerifiedVisualBody = nullptr;
    VerifiedVisualHead = nullptr;
    LastAdapterStatus = TEXT("DG master proxy fallback active.");
}

bool UDiscGolfMetaHumanAvatarBackendComponent::ValidateConfiguredVisual(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    const UDiscGolfAvatarBackendProfile* ExpectedProfile,
    USkeletalMeshComponent*& OutBody,
    USkeletalMeshComponent*& OutHead,
    FString& OutStatus) const
{
    OutBody = nullptr;
    OutHead = nullptr;
    OutStatus.Reset();
    if (!IsRegistered() || !IsValid(VisualActor) || !IsValid(AnimationSourceMesh)
        || !IsValid(ExpectedProfile) || !AnimationSourceMesh->IsRegistered()
        || !AnimationSourceMesh->GetSkeletalMeshAsset()
        || GetWorld() != AnimationSourceMesh->GetWorld())
    {
        OutStatus = TEXT("DG animation source is not usable.");
        return false;
    }
    const AActor* Owner = GetOwner();
    const USceneComponent* VisualRoot = VisualActor->GetRootComponent();
    if (!IsValid(Owner) || VisualActor->GetOwner() != Owner
        || VisualActor->GetWorld() != AnimationSourceMesh->GetWorld()
        || !VisualRoot || !VisualRoot->IsAttachedTo(AnimationSourceMesh))
    {
        OutStatus = TEXT("Assembled visual owner/world/source attachment is invalid.");
        return false;
    }

    bool bDuplicateBody = false;
    bool bDuplicateHead = false;
    OutBody = Session8FindUniqueTaggedSkeletalMesh(
        VisualActor, ExpectedProfile->VisualBodyComponentTag, bDuplicateBody);
    OutHead = Session8FindUniqueTaggedSkeletalMesh(
        VisualActor, ExpectedProfile->VisualHeadComponentTag, bDuplicateHead);
    if (bDuplicateBody || bDuplicateHead || !OutBody || !OutHead
        || OutBody == OutHead)
    {
        OutStatus = TEXT("Assembled visual must expose one distinct tagged body and head.");
        return false;
    }
    if (!OutBody->IsRegistered() || !OutHead->IsRegistered()
        || !OutBody->GetSkeletalMeshAsset() || !OutHead->GetSkeletalMeshAsset())
    {
        OutStatus = TEXT("Tagged assembled body/head skeletal meshes are not ready.");
        return false;
    }
    if (!OutBody->GetAnimClass())
    {
        OutStatus = TEXT("Tagged assembled body has no retarget animation class.");
        return false;
    }

    Session8EnforceTaggedMeshSafety(OutBody);
    Session8EnforceTaggedMeshSafety(OutHead);
    OutBody->AddTickPrerequisiteComponent(AnimationSourceMesh);
    OutHead->AddTickPrerequisiteComponent(AnimationSourceMesh);
    return true;
}
