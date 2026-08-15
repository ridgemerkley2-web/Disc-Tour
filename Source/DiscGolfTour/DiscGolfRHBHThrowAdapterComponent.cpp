#include "DiscGolfRHBHThrowAdapterComponent.h"

#include "DiscGolfThrowComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UDiscGolfRHBHThrowAdapterComponent::UDiscGolfRHBHThrowAdapterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
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
    AuthoritativeLaunchDelegate.Unbind();
    Super::EndPlay(EndPlayReason);
}

void UDiscGolfRHBHThrowAdapterComponent::Configure(
    UDiscGolfThrowComponent* InFrameworkThrowComponent,
    USkeletalMeshComponent* InCharacterMesh,
    UStaticMeshComponent* InHeldDiscVisual)
{
    if (FrameworkThrowComponent != InFrameworkThrowComponent)
    {
        UnbindFrameworkDelegates();
        FrameworkThrowComponent = InFrameworkThrowComponent;
    }

    CharacterMesh = InCharacterMesh;
    HeldDiscVisual = InHeldDiscVisual;
    BindFrameworkDelegates();

    if (!Transaction.IsActive())
    {
        SetHeldDiscVisible(false);
    }
}

bool UDiscGolfRHBHThrowAdapterComponent::TryBeginRHBHThrow(
    const FThrowCommand& AuthoritativeCommand)
{
    // Session 3 deliberately owns one slice only. Other styles/contexts retain
    // their existing immediate gameplay path.
    if (AuthoritativeCommand.ThrowStyle != EThrowStyle::Backhand
        || AuthoritativeCommand.ShotContext != EDiscShotContext::Drive
        || !IsReadyForAnimatedThrow())
    {
        return false;
    }

    if (!Transaction.Begin(AuthoritativeCommand))
    {
        return false;
    }

    // Keep the exact authoritative snapshot for diagnostics/history. The
    // transaction owns the immutable copy used by the launch callback.
    LastAuthoritativeCommand = AuthoritativeCommand;
    bLastLaunchAccepted = false;

    // These values drive character presentation only. They are direct copies;
    // the framework's suggested speed/spin and its release intent are ignored.
    FDGThrowIntent PresentationIntent;
    PresentationIntent.ThrowType = EDGThrowType::Backhand;
    PresentationIntent.Power01 = AuthoritativeCommand.Power01;
    PresentationIntent.HyzerDegrees = AuthoritativeCommand.HyzerDeg;
    PresentationIntent.NoseDegrees = AuthoritativeCommand.NoseAngleDeg;
    PresentationIntent.AimYawDegrees = AuthoritativeCommand.Direction.Rotation().Yaw;
    FrameworkThrowComponent->SetThrowIntent(PresentationIntent);

    SetHeldDiscVisible(true);
    FrameworkThrowComponent->BeginThrow();
    ArmWatchdog(Transaction.GetAttemptSerial());
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

void UDiscGolfRHBHThrowAdapterComponent::ArmWatchdog(uint64 AttemptSerial)
{
    ClearWatchdog();
    if (!GetWorld() || WatchdogTimeoutSeconds <= 0.0f)
    {
        return;
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
    OnThrowRecovered.Broadcast(
        static_cast<int64>(Transaction.GetAttemptSerial()),
        bDiscWasReleased);
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
#endif
