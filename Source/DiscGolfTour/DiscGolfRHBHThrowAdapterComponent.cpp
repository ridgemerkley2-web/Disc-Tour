#include "DiscGolfRHBHThrowAdapterComponent.h"

#include "DiscBagComponent.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfMath.h"
#include "DiscGolfThrowComponent.h"
#include "ThrowControllerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

namespace
{
FName PlasticIdForCommand(EDiscPlastic Plastic)
{
    switch (Plastic)
    {
        case EDiscPlastic::Base: return TEXT("Base");
        case EDiscPlastic::Tour: return TEXT("Tour");
        case EDiscPlastic::Crystal: return TEXT("Crystal");
        default: return NAME_None;
    }
}

bool HasExactSelectedEquipmentProvenance(
    const UDiscBagComponent* DiscBag,
    const FThrowCommand& Candidate)
{
    FDGDiscInstance SelectedInstance;
    const FName ExpectedPlasticId = PlasticIdForCommand(Candidate.Plastic);
    if (!DiscBag
        || ExpectedPlasticId.IsNone()
        || !DiscBag->GetSelectedDiscInstance(SelectedInstance)
        || !Candidate.DiscInstanceId.IsValid()
        || !SelectedInstance.InstanceId.IsValid()
        || SelectedInstance.InstanceId != Candidate.DiscInstanceId
        || SelectedInstance.DiscDefinitionId != Candidate.MoldId
        || SelectedInstance.PlasticId != ExpectedPlasticId
        || DiscBag->GetSelectedMoldId() != Candidate.MoldId
        || DiscBag->GetSelectedPlastic() != Candidate.Plastic)
    {
        return false;
    }

    return true;
}
}

UDiscGolfRHBHThrowAdapterComponent::UDiscGolfRHBHThrowAdapterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UDiscGolfRHBHThrowAdapterComponent::IsSelectedEquipmentProvenanceExact(
    const FThrowCommand& Command,
    const UDiscBagComponent* DiscBag)
{
    return HasExactSelectedEquipmentProvenance(DiscBag, Command);
}

void UDiscGolfRHBHThrowAdapterComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!FrameworkThrowComponent)
    {
        FrameworkThrowComponent = GetOwner()
            ? GetOwner()->FindComponentByClass<UDiscGolfThrowComponent>()
            : nullptr;
    }

    BindFrameworkDelegates();
    SetHeldDiscVisible(false);
}

void UDiscGolfRHBHThrowAdapterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearWatchdog();
    UnbindFrameworkDelegates();
    SetHeldDiscVisible(false);
    ReleaseEquipmentMutationLock();
    AuthoritativeLaunchDelegate.Unbind();
    Super::EndPlay(EndPlayReason);
}

void UDiscGolfRHBHThrowAdapterComponent::Configure(
    UDiscGolfThrowComponent* InFrameworkThrowComponent,
    USkeletalMeshComponent* InCharacterMesh,
    UStaticMeshComponent* InHeldDiscVisual)
{
    if (Transaction.IsActive())
    {
        return;
    }

    if (FrameworkThrowComponent != InFrameworkThrowComponent)
    {
        UnbindFrameworkDelegates();
        FrameworkThrowComponent = InFrameworkThrowComponent;
    }

    CharacterMesh = InCharacterMesh;
    HeldDiscVisual = InHeldDiscVisual;
    BindFrameworkDelegates();

    SetHeldDiscVisible(false);
}

void UDiscGolfRHBHThrowAdapterComponent::ConfigureIngressContracts(
    UThrowControllerComponent* InThrowController,
    UDiscBagComponent* InDiscBag)
{
    if (Transaction.IsActive())
    {
        return;
    }

    ReleaseEquipmentMutationLock();
    ThrowController = InThrowController;
    DiscBag = InDiscBag;
}

bool UDiscGolfRHBHThrowAdapterComponent::IsCommandEligibleForAnimatedRHBH(
    const FThrowCommand& Command,
    EDGHandedness CurrentProfileHandedness,
    EDiscShotContext ExpectedShotContext)
{
    return DiscGolfMath::IsThrowCommandValid(Command)
        && Command.ThrowStyle == EThrowStyle::Backhand
        && Command.Handedness == EDGHandedness::Right
        && CurrentProfileHandedness == EDGHandedness::Right
        && DiscGolfMath::IsValidDiscShotContext(ExpectedShotContext)
        && Command.ShotContext == ExpectedShotContext;
}

bool UDiscGolfRHBHThrowAdapterComponent::IsWatchdogTimeoutValid(float TimeoutSeconds)
{
    constexpr float MinimumWatchdogSeconds = 0.25f;
    constexpr float MaximumWatchdogSeconds = 60.0f;
    return FMath::IsFinite(TimeoutSeconds)
        && TimeoutSeconds >= MinimumWatchdogSeconds
        && TimeoutSeconds <= MaximumWatchdogSeconds;
}

FDGThrowIntent UDiscGolfRHBHThrowAdapterComponent::
    BuildPresentationIntentForAnimatedRHBH(const FThrowCommand& Command)
{
    FDGThrowIntent PresentationIntent;
    PresentationIntent.ThrowType = EDGThrowType::Backhand;
    PresentationIntent.Power01 = Command.Power01;
    PresentationIntent.HyzerDegrees = Command.HyzerDeg;
    PresentationIntent.NoseDegrees = Command.NoseAngleDeg;
    // The rig consumes AimYawDegrees as a local cosmetic offset. The immutable
    // command direction is world-space launch authority and must not be reused
    // as a wrist delta when the preview becomes committed.
    PresentationIntent.AimYawDegrees = 0.0f;
    return PresentationIntent;
}

bool UDiscGolfRHBHThrowAdapterComponent::TryBeginRHBHThrow(
    const FThrowCommand& AuthoritativeCommand)
{
    if (!IsReadyForAnimatedThrow()
        || !TryBeginValidatedTransaction(AuthoritativeCommand))
    {
        return false;
    }

    // These values drive character presentation only. They are direct copies;
    // the framework's suggested speed/spin and its release intent are ignored.
    const FDGThrowIntent PresentationIntent =
        BuildPresentationIntentForAnimatedRHBH(AuthoritativeCommand);
    FrameworkThrowComponent->SetThrowIntent(PresentationIntent);

    SetHeldDiscVisible(true);
    FrameworkThrowComponent->BeginThrow();
    if (!ArmWatchdog(Transaction.GetAttemptSerial()))
    {
        // A transaction without a fixed recovery authority could permanently
        // retain both animation and equipment locks. Recover immediately.
        RecoverInterruptedThrow();
        return false;
    }
    return true;
}

bool UDiscGolfRHBHThrowAdapterComponent::CancelBeforeRelease()
{
    if (!Transaction.CancelBeforeRelease())
    {
        return false;
    }

    ClearWatchdog();
    SetHeldDiscVisible(false);
    if (FrameworkThrowComponent)
    {
        FrameworkThrowComponent->CancelThrow();
    }
    CompleteRecovery(false);
    return true;
}

bool UDiscGolfRHBHThrowAdapterComponent::RecoverInterruptedThrow()
{
    const bool bDiscWasReleased = Transaction.HasCommittedRelease();
    if (!Transaction.RecoverFromInterruption())
    {
        return false;
    }

    ClearWatchdog();
    SetHeldDiscVisible(false);
    if (FrameworkThrowComponent)
    {
        FrameworkThrowComponent->CancelThrow();
    }
    CompleteRecovery(bDiscWasReleased);
    return true;
}

bool UDiscGolfRHBHThrowAdapterComponent::IsReadyForAnimatedThrow() const
{
    if (!FrameworkThrowComponent
        || FrameworkThrowComponent->bThrowActive
        || !FrameworkThrowComponent->CharacterProfile
        || FrameworkThrowComponent->CharacterProfile->Handedness != EDGHandedness::Right
        || !GetWorld()
        || !IsWatchdogTimeoutValid(WatchdogTimeoutSeconds)
        || !ThrowController
        || !DiscBag
        || DiscBag->IsEquipmentMutationLocked()
        || !CharacterMesh
        || !HeldDiscVisual
        || !HeldDiscVisual->GetStaticMesh()
        || !AuthoritativeLaunchDelegate.IsBound())
    {
        return false;
    }

    const FName GripBone = FrameworkThrowComponent->GetActiveDiscGripBone();
    return GripBone == FName(TEXT("disc_grip_r"))
        && CharacterMesh->DoesSocketExist(GripBone)
        && HeldDiscVisual->GetAttachParent() == CharacterMesh
        && HeldDiscVisual->GetAttachSocketName() == GripBone;
}

void UDiscGolfRHBHThrowAdapterComponent::HandleFrameworkDiscRelease(FDGReleaseData ReleaseData)
{
    // GripWorldTransform is intentionally the only release-data field consumed.
    const FTransform GripWorldTransform = ReleaseData.GripWorldTransform;

    const bool bCommitted = Transaction.TryCommitRelease(
        [this, &GripWorldTransform](const FThrowCommand& ExactCachedCommand)
        {
            // Commit has already occurred before this callback. Hide the held
            // visual before gameplay authority can synchronously spawn a disc.
            SetHeldDiscVisible(false);

            bLastLaunchAccepted = GripWorldTransform.IsValid()
                && AuthoritativeLaunchDelegate.IsBound()
                && AuthoritativeLaunchDelegate.Execute(ExactCachedCommand, GripWorldTransform);
        });

    if (!bCommitted)
    {
        return;
    }

    OnReleaseCommitted.Broadcast(
        static_cast<int64>(Transaction.GetAttemptSerial()),
        bLastLaunchAccepted);
}

void UDiscGolfRHBHThrowAdapterComponent::HandleFrameworkThrowFinished()
{
    const bool bDiscWasReleased = Transaction.HasCommittedRelease();
    if (!Transaction.Finish())
    {
        return;
    }

    ClearWatchdog();
    SetHeldDiscVisible(false);
    CompleteRecovery(bDiscWasReleased);
}

void UDiscGolfRHBHThrowAdapterComponent::BindFrameworkDelegates()
{
    if (!FrameworkThrowComponent)
    {
        return;
    }

    FrameworkThrowComponent->OnDiscRelease.RemoveDynamic(
        this,
        &UDiscGolfRHBHThrowAdapterComponent::HandleFrameworkDiscRelease);
    FrameworkThrowComponent->OnDiscRelease.AddUniqueDynamic(
        this,
        &UDiscGolfRHBHThrowAdapterComponent::HandleFrameworkDiscRelease);
    FrameworkThrowComponent->OnThrowFinished.RemoveDynamic(
        this,
        &UDiscGolfRHBHThrowAdapterComponent::HandleFrameworkThrowFinished);
    FrameworkThrowComponent->OnThrowFinished.AddUniqueDynamic(
        this,
        &UDiscGolfRHBHThrowAdapterComponent::HandleFrameworkThrowFinished);
}

void UDiscGolfRHBHThrowAdapterComponent::UnbindFrameworkDelegates()
{
    if (!FrameworkThrowComponent)
    {
        return;
    }

    FrameworkThrowComponent->OnDiscRelease.RemoveDynamic(
        this,
        &UDiscGolfRHBHThrowAdapterComponent::HandleFrameworkDiscRelease);
    FrameworkThrowComponent->OnThrowFinished.RemoveDynamic(
        this,
        &UDiscGolfRHBHThrowAdapterComponent::HandleFrameworkThrowFinished);
}

bool UDiscGolfRHBHThrowAdapterComponent::ArmWatchdog(uint64 AttemptSerial)
{
    ClearWatchdog();
    if (!GetWorld() || !IsWatchdogTimeoutValid(WatchdogTimeoutSeconds))
    {
        return false;
    }

    FTimerDelegate WatchdogDelegate;
    WatchdogDelegate.BindUObject(
        this,
        &UDiscGolfRHBHThrowAdapterComponent::HandleWatchdog,
        AttemptSerial);
    GetWorld()->GetTimerManager().SetTimer(
        WatchdogTimer,
        WatchdogDelegate,
        WatchdogTimeoutSeconds,
        false);
    return WatchdogTimer.IsValid();
}

void UDiscGolfRHBHThrowAdapterComponent::ClearWatchdog()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(WatchdogTimer);
    }
    WatchdogTimer.Invalidate();
}

void UDiscGolfRHBHThrowAdapterComponent::HandleWatchdog(uint64 ExpectedAttemptSerial)
{
    // A stale timer from an earlier montage can never recover a later throw.
    const bool bDiscWasReleased = Transaction.HasCommittedRelease();
    if (!Transaction.RecoverFromWatchdog(ExpectedAttemptSerial))
    {
        return;
    }

    ClearWatchdog();
    SetHeldDiscVisible(false);
    if (FrameworkThrowComponent)
    {
        FrameworkThrowComponent->CancelThrow();
    }
    CompleteRecovery(bDiscWasReleased);
}

void UDiscGolfRHBHThrowAdapterComponent::CompleteRecovery(bool bDiscWasReleased)
{
    ReleaseEquipmentMutationLock();
    OnThrowRecovered.Broadcast(
        static_cast<int64>(Transaction.GetAttemptSerial()),
        bDiscWasReleased);
}

bool UDiscGolfRHBHThrowAdapterComponent::TryBeginValidatedTransaction(
    const FThrowCommand& Command)
{
    const bool bUsesOwnerAuthoritativeLaunch = GetOwner()
        && AuthoritativeLaunchDelegate.IsBoundToObject(GetOwner());
    if (Transaction.IsActive()
        || !FrameworkThrowComponent
        || !FrameworkThrowComponent->CharacterProfile
        || !ThrowController
        || !DiscBag
        || !AuthoritativeLaunchDelegate.IsBound()
        || !IsCommandEligibleForAnimatedRHBH(
            Command,
            FrameworkThrowComponent->CharacterProfile->Handedness,
            ThrowController->GetShotContext())
        // Normal player release is bound back to the owning Pawn and must
        // carry the exact selected equipment identity before montage/lock.
        // Developer visual fixtures bind a non-owner presentation callback;
        // those synthetic commands never enter GameMode launch authority.
        || (bUsesOwnerAuthoritativeLaunch
            && !IsSelectedEquipmentProvenanceExact(Command, DiscBag))
        || !TryAcquireEquipmentMutationLock())
    {
        return false;
    }

    if (!Transaction.Begin(Command))
    {
        ReleaseEquipmentMutationLock();
        return false;
    }

    // Keep the exact authoritative snapshot for diagnostics/history. The
    // transaction owns the immutable copy used by the launch callback.
    LastAuthoritativeCommand = Command;
    bLastLaunchAccepted = false;
    return true;
}

bool UDiscGolfRHBHThrowAdapterComponent::TryAcquireEquipmentMutationLock()
{
    if (!DiscBag || bOwnsEquipmentMutationLock || DiscBag->IsEquipmentMutationLocked())
    {
        return false;
    }

    DiscBag->SetEquipmentMutationLocked(true);
    bOwnsEquipmentMutationLock = true;
    return true;
}

void UDiscGolfRHBHThrowAdapterComponent::ReleaseEquipmentMutationLock()
{
    if (bOwnsEquipmentMutationLock && DiscBag)
    {
        DiscBag->SetEquipmentMutationLocked(false);
    }
    bOwnsEquipmentMutationLock = false;
}

void UDiscGolfRHBHThrowAdapterComponent::SetHeldDiscVisible(bool bVisible) const
{
    if (!HeldDiscVisual)
    {
        return;
    }

    HeldDiscVisual->SetHiddenInGame(!bVisible, true);
    HeldDiscVisual->SetVisibility(bVisible, true);
}

#if WITH_DEV_AUTOMATION_TESTS
void UDiscGolfRHBHThrowAdapterComponent::TriggerWatchdogForTesting()
{
    HandleWatchdog(Transaction.GetAttemptSerial());
}

bool UDiscGolfRHBHThrowAdapterComponent::TryBeginValidatedTransactionForTesting(
    const FThrowCommand& Command)
{
    return TryBeginValidatedTransaction(Command);
}

void UDiscGolfRHBHThrowAdapterComponent::TriggerReleaseForTesting(
    const FTransform& GripWorldTransform)
{
    FDGReleaseData ReleaseData;
    ReleaseData.GripWorldTransform = GripWorldTransform;
    HandleFrameworkDiscRelease(ReleaseData);
}
#endif
