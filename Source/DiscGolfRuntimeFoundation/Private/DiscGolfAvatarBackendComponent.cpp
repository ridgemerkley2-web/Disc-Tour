#include "DiscGolfAvatarBackendComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UDiscGolfAvatarBackendComponent::UDiscGolfAvatarBackendComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UDiscGolfAvatarBackendComponent::BuildVisualBackend(
    USkeletalMeshComponent* AnimationSourceMesh,
    const FDGFullCharacterCustomization& Customization)
{
    if (!IsValid(BackendProfile) || !IsValid(AnimationSourceMesh) || !IsValid(GetWorld()))
    {
        return false;
    }
    UClass* VisualClass = BackendProfile->VisualActorClass.LoadSynchronous();
    if (!VisualClass || !VisualClass->IsChildOf(AActor::StaticClass())
        || VisualClass->IsChildOf(APawn::StaticClass())
        || VisualClass->IsChildOf(AController::StaticClass()))
    {
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = GetOwner();
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Candidate = GetWorld()->SpawnActor<AActor>(VisualClass, FTransform::Identity, SpawnParameters);
    if (!IsValid(Candidate))
    {
        return false;
    }
    Candidate->SetActorEnableCollision(false);
    if (USceneComponent* Root = Candidate->GetRootComponent())
    {
        Root->AttachToComponent(AnimationSourceMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }

    if (!ConfigureVisualBackend(Candidate, AnimationSourceMesh, Customization)
        || !FinalizeVisualBackendActivation(Candidate, AnimationSourceMesh, Customization))
    {
        Candidate->Destroy();
        return false;
    }

    if (IsValid(ActiveVisualActor) && ActiveVisualActor != Candidate)
    {
        ActiveVisualActor->Destroy();
    }
    ActiveVisualActor = Candidate;
    ActiveAnimationSourceMesh = AnimationSourceMesh;
    ActiveBackendProfile = BackendProfile;
    BackendState.BackendId = BackendProfile->BackendId;
    BackendState.Backend = BackendProfile->Backend;
    BackendState.bVisualReady = true;
    BackendState.Status = TEXT("Project-owned visual backend active.");
    OnAvatarBackendReady.Broadcast(BackendState);
    return true;
}

bool UDiscGolfAvatarBackendComponent::ApplyCustomizationToVisual(
    const FDGFullCharacterCustomization& Customization)
{
    return IsVisualBackendReady() && ApplyVisualCustomization(ActiveVisualActor, Customization);
}

void UDiscGolfAvatarBackendComponent::DestroyVisualBackend()
{
    if (IsValid(ActiveVisualActor))
    {
        ActiveVisualActor->Destroy();
    }
    ActiveVisualActor = nullptr;
    ActiveAnimationSourceMesh = nullptr;
    ActiveBackendProfile = nullptr;
    BackendState = FDGAvatarBackendState();
}

bool UDiscGolfAvatarBackendComponent::ConfigureVisualBackend_Implementation(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    const FDGFullCharacterCustomization& Customization)
{
    (void)Customization;
    return IsValid(VisualActor) && IsValid(AnimationSourceMesh);
}

bool UDiscGolfAvatarBackendComponent::ApplyVisualCustomization_Implementation(
    AActor* VisualActor,
    const FDGFullCharacterCustomization& Customization)
{
    (void)Customization;
    return IsValid(VisualActor);
}

bool UDiscGolfAvatarBackendComponent::FinalizeVisualBackendActivation(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    const FDGFullCharacterCustomization& Customization)
{
    (void)Customization;
    return IsValid(VisualActor) && IsValid(AnimationSourceMesh)
        && VisualActor->GetWorld() == AnimationSourceMesh->GetWorld();
}
