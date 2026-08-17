#include "DiscGolfAvatarBackendComponent.h"
#include "DiscGolfAvatarBackendProfile.h"
#include "Components/PrimitiveComponent.h"
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
    if (bBuildInProgress || bApplyInProgress)
    {
        return false;
    }
    TGuardValue<bool> BuildGuard(bBuildInProgress, true);

    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    UDiscGolfAvatarBackendProfile* CandidateProfile = BackendProfile.Get();
    if (!IsRegistered() || !IsValid(Owner) || Owner->IsActorBeingDestroyed() || !World
        || !IsValid(CandidateProfile) || CandidateProfile->BackendId.IsNone()
        || !IsAnimationSourceUsable(AnimationSourceMesh, World))
    {
        return false;
    }

    UClass* VisualClass = CandidateProfile->VisualActorClass.LoadSynchronous();
    if (!VisualClass || !VisualClass->IsChildOf(AActor::StaticClass())
        || VisualClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return false;
    }
    if (!IsRegistered() || GetOwner() != Owner || GetWorld() != World
        || !IsValid(Owner) || Owner->IsActorBeingDestroyed()
        || BackendProfile.Get() != CandidateProfile
        || !IsValid(CandidateProfile)
        || !IsAnimationSourceUsable(AnimationSourceMesh, World))
    {
        return false;
    }

    const FTransform SpawnTransform = AnimationSourceMesh->GetComponentTransform();
    AActor* CandidateActor = World->SpawnActorDeferred<AActor>(
        VisualClass,
        SpawnTransform,
        Owner,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );
    if (!CandidateActor)
    {
        return false;
    }
    PendingVisualActor = CandidateActor;

    // Establish the safety boundary before Blueprint construction/BeginPlay,
    // then enforce it again after project configuration can add components.
    CandidateActor->SetActorHiddenInGame(true);
    CandidateActor->SetActorEnableCollision(false);
    CandidateActor->FinishSpawning(SpawnTransform);
    if (!IsValid(CandidateActor))
    {
        PendingVisualActor = nullptr;
        return false;
    }
    if (!IsRegistered() || GetOwner() != Owner || GetWorld() != World
        || !IsValid(Owner) || Owner->IsActorBeingDestroyed()
        || BackendProfile.Get() != CandidateProfile
        || !IsValid(CandidateProfile)
        || !IsAnimationSourceUsable(AnimationSourceMesh, World))
    {
        PendingVisualActor = nullptr;
        DestroyVisualActor(CandidateActor);
        return false;
    }
    EnforcePresentationOnly(CandidateActor, true);

    if (!CandidateActor->GetRootComponent()
        || !CandidateActor->AttachToComponent(
            AnimationSourceMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale))
    {
        PendingVisualActor = nullptr;
        DestroyVisualActor(CandidateActor);
        return false;
    }
    AddSourceTickPrerequisites(CandidateActor, AnimationSourceMesh);
    if (!IsRegistered() || GetOwner() != Owner || GetWorld() != World
        || !IsValid(Owner) || Owner->IsActorBeingDestroyed()
        || BackendProfile.Get() != CandidateProfile
        || !IsValid(CandidateProfile)
        || !IsAnimationSourceUsable(AnimationSourceMesh, World))
    {
        PendingVisualActor = nullptr;
        DestroyVisualActor(CandidateActor);
        return false;
    }

    const bool bConfigured = ConfigureVisualBackend(
        CandidateActor,
        AnimationSourceMesh,
        Customization
    );
    EnforcePresentationOnly(CandidateActor, true);
    AddSourceTickPrerequisites(CandidateActor, AnimationSourceMesh);

    const USceneComponent* CandidateRoot = IsValid(CandidateActor)
        ? CandidateActor->GetRootComponent() : nullptr;
    const bool bCandidateReady = bConfigured
        && IsRegistered()
        && IsValid(Owner)
        && !Owner->IsActorBeingDestroyed()
        && GetOwner() == Owner
        && GetWorld() == World
        && IsValid(CandidateProfile)
        && IsAnimationSourceUsable(AnimationSourceMesh, World)
        && CandidateRoot
        && !CandidateActor->IsActorBeingDestroyed()
        && CandidateRoot->IsAttachedTo(AnimationSourceMesh);
    if (!bCandidateReady)
    {
        PendingVisualActor = nullptr;
        DestroyVisualActor(CandidateActor);
        return false;
    }

    AActor* PreviousActor = SpawnedVisualActor.Get();
    SpawnedVisualActor = CandidateActor;
    PendingVisualActor = nullptr;
    ActiveBackendProfile = CandidateProfile;
    ActiveAnimationSourceMesh = AnimationSourceMesh;
    ActiveBackendState.Backend = CandidateProfile->Backend;
    ActiveBackendState.BackendId = CandidateProfile->BackendId;
    ActiveBackendState.bVisualReady = true;

    if (IsValid(PreviousActor) && PreviousActor != CandidateActor)
    {
        PreviousActor->SetActorHiddenInGame(true);
    }
    CandidateActor->SetActorHiddenInGame(false);
    if (PreviousActor != CandidateActor)
    {
        DestroyVisualActor(PreviousActor);
    }

    const FDGAvatarBackendState ReadyState = GetAvatarBackendState();
    if (!ReadyState.bVisualReady)
    {
        DestroyVisualBackend();
        return false;
    }
    OnAvatarBackendReady.Broadcast(ReadyState);
    return IsVisualBackendReady();
}

bool UDiscGolfAvatarBackendComponent::ApplyCustomizationToVisual(
    const FDGFullCharacterCustomization& Customization)
{
    if (bBuildInProgress || bApplyInProgress || !IsVisualBackendReady())
    {
        return false;
    }
    TGuardValue<bool> ApplyGuard(bApplyInProgress, true);

    AActor* ActiveActor = SpawnedVisualActor.Get();
    USkeletalMeshComponent* AnimationSourceMesh = ActiveAnimationSourceMesh.Get();
    const bool bWasHidden = ActiveActor->IsHidden();
    const bool bApplied = ApplyVisualCustomization(ActiveActor, Customization);
    if (!IsValid(ActiveActor))
    {
        ResetActiveBackendState();
        return false;
    }

    EnforcePresentationOnly(ActiveActor, bWasHidden);
    AddSourceTickPrerequisites(ActiveActor, AnimationSourceMesh);
    if (!IsVisualBackendReady())
    {
        DestroyVisualBackend();
        return false;
    }
    return bApplied;
}

void UDiscGolfAvatarBackendComponent::DestroyVisualBackend()
{
    AActor* PendingActor = PendingVisualActor.Get();
    AActor* ActiveActor = SpawnedVisualActor.Get();
    PendingVisualActor = nullptr;
    ResetActiveBackendState();

    if (PendingActor != ActiveActor)
    {
        DestroyVisualActor(PendingActor);
    }
    DestroyVisualActor(ActiveActor);
}

bool UDiscGolfAvatarBackendComponent::IsVisualBackendReady() const
{
    const AActor* Owner = GetOwner();
    const UWorld* World = GetWorld();
    const AActor* ActiveActor = SpawnedVisualActor.Get();
    const USkeletalMeshComponent* AnimationSourceMesh = ActiveAnimationSourceMesh.Get();
    const USceneComponent* ActiveRoot = IsValid(ActiveActor)
        ? ActiveActor->GetRootComponent() : nullptr;
    return ActiveBackendState.bVisualReady
        && IsRegistered()
        && IsValid(Owner)
        && !Owner->IsActorBeingDestroyed()
        && World
        && IsValid(ActiveBackendProfile)
        && IsAnimationSourceUsable(AnimationSourceMesh, World)
        && IsValid(ActiveActor)
        && !ActiveActor->IsActorBeingDestroyed()
        && ActiveActor->GetOwner() == Owner
        && ActiveActor->GetWorld() == World
        && ActiveRoot
        && ActiveRoot->IsAttachedTo(AnimationSourceMesh);
}

FDGAvatarBackendState UDiscGolfAvatarBackendComponent::GetAvatarBackendState() const
{
    FDGAvatarBackendState State = ActiveBackendState;
    State.bVisualReady = IsVisualBackendReady();
    return State;
}

AActor* UDiscGolfAvatarBackendComponent::GetActiveVisualActor() const
{
    return IsVisualBackendReady() ? SpawnedVisualActor.Get() : nullptr;
}

UDiscGolfAvatarBackendProfile*
UDiscGolfAvatarBackendComponent::GetActiveBackendProfile() const
{
    return IsVisualBackendReady() ? ActiveBackendProfile.Get() : nullptr;
}

USkeletalMeshComponent*
UDiscGolfAvatarBackendComponent::GetActiveAnimationSourceMesh() const
{
    return IsVisualBackendReady() ? ActiveAnimationSourceMesh.Get() : nullptr;
}

bool UDiscGolfAvatarBackendComponent::ConfigureVisualBackend_Implementation(
    AActor*,
    USkeletalMeshComponent*,
    const FDGFullCharacterCustomization&)
{
    return false;
}

bool UDiscGolfAvatarBackendComponent::ApplyVisualCustomization_Implementation(
    AActor*,
    const FDGFullCharacterCustomization&)
{
    return false;
}

void UDiscGolfAvatarBackendComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    DestroyVisualBackend();
    Super::EndPlay(EndPlayReason);
}

void UDiscGolfAvatarBackendComponent::OnUnregister()
{
    DestroyVisualBackend();
    Super::OnUnregister();
}

bool UDiscGolfAvatarBackendComponent::IsAnimationSourceUsable(
    const USkeletalMeshComponent* AnimationSourceMesh,
    const UWorld* ExpectedWorld)
{
    return IsValid(AnimationSourceMesh)
        && ExpectedWorld
        && AnimationSourceMesh->GetWorld() == ExpectedWorld
        && AnimationSourceMesh->IsRegistered()
        && AnimationSourceMesh->GetSkeletalMeshAsset() != nullptr;
}

void UDiscGolfAvatarBackendComponent::EnforcePresentationOnly(
    AActor* VisualActor,
    bool bHidden)
{
    if (!IsValid(VisualActor) || VisualActor->IsActorBeingDestroyed())
    {
        return;
    }

    VisualActor->SetActorHiddenInGame(bHidden);
    VisualActor->SetActorEnableCollision(false);

    TInlineComponentArray<UActorComponent*> ActorComponents;
    VisualActor->GetComponents(ActorComponents);
    for (UActorComponent* Component : ActorComponents)
    {
        if (IsValid(Component))
        {
            Component->SetCanEverAffectNavigation(false);
        }
    }

    TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
    VisualActor->GetComponents(PrimitiveComponents);
    for (UPrimitiveComponent* Primitive : PrimitiveComponents)
    {
        if (!IsValid(Primitive))
        {
            continue;
        }
        Primitive->SetSimulatePhysics(false);
        Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Primitive->SetGenerateOverlapEvents(false);
        Primitive->SetCanEverAffectNavigation(false);
    }
}

void UDiscGolfAvatarBackendComponent::AddSourceTickPrerequisites(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh)
{
    if (!IsValid(VisualActor) || !IsValid(AnimationSourceMesh))
    {
        return;
    }

    VisualActor->AddTickPrerequisiteComponent(AnimationSourceMesh);
    TInlineComponentArray<USkeletalMeshComponent*> TargetMeshes;
    VisualActor->GetComponents(TargetMeshes);
    for (USkeletalMeshComponent* TargetMesh : TargetMeshes)
    {
        if (IsValid(TargetMesh) && TargetMesh != AnimationSourceMesh)
        {
            TargetMesh->AddTickPrerequisiteComponent(AnimationSourceMesh);
        }
    }
}

void UDiscGolfAvatarBackendComponent::DestroyVisualActor(AActor* VisualActor)
{
    if (!IsValid(VisualActor) || VisualActor->IsActorBeingDestroyed())
    {
        return;
    }
    VisualActor->SetActorHiddenInGame(true);
    VisualActor->SetActorEnableCollision(false);
    VisualActor->Destroy();
}

void UDiscGolfAvatarBackendComponent::ResetActiveBackendState()
{
    SpawnedVisualActor = nullptr;
    ActiveBackendProfile = nullptr;
    ActiveAnimationSourceMesh = nullptr;
    ActiveBackendState = FDGAvatarBackendState();
}
