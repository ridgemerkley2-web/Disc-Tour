#include "DiscGolfAvatarBackendComponent.h"
#include "DiscGolfAvatarBackendProfile.h"
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
    DestroyVisualBackend();

    if (!BackendProfile || !GetOwner() || !GetWorld())
    {
        return false;
    }

    UClass* VisualClass = BackendProfile->VisualActorClass.LoadSynchronous();
    if (!VisualClass)
    {
        return false;
    }

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    SpawnedVisualActor = GetWorld()->SpawnActor<AActor>(
        VisualClass,
        GetOwner()->GetActorTransform(),
        Params
    );

    if (!SpawnedVisualActor)
    {
        return false;
    }

    if (USceneComponent* OwnerRoot = GetOwner()->GetRootComponent())
    {
        SpawnedVisualActor->AttachToComponent(
            OwnerRoot,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale
        );
    }

    ConfigureVisualBackend(
        SpawnedVisualActor,
        AnimationSourceMesh,
        Customization
    );

    FDGAvatarBackendState State;
    State.Backend = BackendProfile->Backend;
    State.BackendId = BackendProfile->BackendId;
    State.bVisualReady = true;
    OnAvatarBackendReady.Broadcast(State);
    return true;
}

void UDiscGolfAvatarBackendComponent::ApplyCustomizationToVisual(
    const FDGFullCharacterCustomization& Customization)
{
    if (SpawnedVisualActor)
    {
        ApplyVisualCustomization(SpawnedVisualActor, Customization);
    }
}

void UDiscGolfAvatarBackendComponent::DestroyVisualBackend()
{
    if (SpawnedVisualActor)
    {
        SpawnedVisualActor->Destroy();
        SpawnedVisualActor = nullptr;
    }
}
