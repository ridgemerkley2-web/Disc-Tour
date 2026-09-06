#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfRHBHThrowTransaction.h"
#include "TimerManager.h"
#include "DiscGolfRHBHThrowAdapterComponent.generated.h"

class UDiscGolfThrowComponent;
class UDiscBagComponent;
class UThrowControllerComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * The one synchronous hand-off from character presentation to the existing
 * gameplay launch authority. Returning false records that the gameplay gate
 * rejected the request; it never causes the adapter to retry the release.
 */
DECLARE_DELEGATE_RetVal_TwoParams(
    bool,
    FDiscGolfAuthoritativeRHBHLaunchDelegate,
    const FThrowCommand&,
    const FTransform&);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FDiscGolfRHBHAdapterReleaseEvent,
    int64,
    AttemptSerial,
    bool,
    bAuthoritativeLaunchAccepted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FDiscGolfRHBHAdapterRecoveryEvent,
    int64,
    AttemptSerial,
    bool,
    bDiscWasReleased);

/**
 * Project-owned bridge between the installed character framework's animation
 * notifies and DiscGolfTour's authoritative throw path.
 *
 * This component owns only the transaction/latch and held-disc visibility. It
 * does not own the skeletal mesh, held-disc component, montage, flight inputs,
 * gameplay disc, camera, or input context. Its launch delegate receives the
 * exact cached FThrowCommand and only FDGReleaseData::GripWorldTransform.
 */
UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UDiscGolfRHBHThrowAdapterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfRHBHThrowAdapterComponent();

    /**
     * Supplies owner-created components and binds framework release/finish
     * delegates. The adapter references these objects but never creates or
     * replaces them.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 3")
    void Configure(
        UDiscGolfThrowComponent* InFrameworkThrowComponent,
        USkeletalMeshComponent* InCharacterMesh,
        UStaticMeshComponent* InHeldDiscVisual);

    /** Supplies the player-owned context and equipment gates for animated throws. */
    void ConfigureIngressContracts(
        UThrowControllerComponent* InThrowController,
        UDiscBagComponent* InDiscBag);

    /** Pure policy seam shared by runtime ingress and focused automation. */
    static bool IsCommandEligibleForAnimatedRHBH(
        const FThrowCommand& Command,
        EDGHandedness CurrentProfileHandedness,
        EDiscShotContext ExpectedShotContext);
    /** Exact immutable player-equipment admission policy; never repairs identity. */
    static bool IsSelectedEquipmentProvenanceExact(
        const FThrowCommand& Command,
        const UDiscBagComponent* DiscBag);
    static bool IsWatchdogTimeoutValid(float TimeoutSeconds);

    /**
     * Copies only presentation scalars from the immutable command. World-space
     * launch heading deliberately remains outside the rig's local aim offset.
     */
    static FDGThrowIntent BuildPresentationIntentForAnimatedRHBH(
        const FThrowCommand& Command);

    /** Starts an authored right-handed backhand Drive, Approach, or Putt family. */
    bool TryBeginRHBHThrow(const FThrowCommand& AuthoritativeCommand);

    /** Cancels only while the disc is still held; a released throw is immutable. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 3")
    bool CancelBeforeRelease();

    /**
     * Restores the animation lock after a montage interruption. Before release
     * this behaves as cancellation; after release it never requests a new disc.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 3")
    bool RecoverInterruptedThrow();

    /** Single binding point for ADiscGolferPawn -> RequestThrowFromGrip. */
    FDiscGolfAuthoritativeRHBHLaunchDelegate& GetAuthoritativeLaunchDelegate()
    {
        return AuthoritativeLaunchDelegate;
    }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    bool IsReadyForAnimatedThrow() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    bool IsThrowActive() const { return Transaction.IsActive(); }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    bool IsAwaitingRelease() const { return Transaction.IsAwaitingRelease(); }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    bool HasCommittedRelease() const { return Transaction.HasCommittedRelease(); }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    int64 GetAttemptSerial() const { return static_cast<int64>(Transaction.GetAttemptSerial()); }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    int32 GetReleaseCommitCountForAttempt() const
    {
        return Transaction.GetReleaseCommitCountForAttempt();
    }

    /** Short smoke-test alias for the current-attempt release count. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    int32 GetReleaseCommitCount() const
    {
        return Transaction.GetReleaseCommitCountForAttempt();
    }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    int32 GetTotalReleaseCommitCount() const
    {
        return Transaction.GetTotalReleaseCommitCount();
    }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    bool WasLastAuthoritativeLaunchAccepted() const { return bLastLaunchAccepted; }

    /** Retained for validation/history; it is never reconstructed from animation data. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    FThrowCommand GetLastAuthoritativeCommand() const { return LastAuthoritativeCommand; }

    EDiscGolfRHBHThrowTransactionState GetTransactionState() const
    {
        return Transaction.GetState();
    }

    EDiscGolfRHBHThrowRecoveryReason GetRecoveryReason() const
    {
        return Transaction.GetRecoveryReason();
    }

#if WITH_DEV_AUTOMATION_TESTS
    /** Deterministic no-wait seam for the bounded recovery automation fixture. */
    void TriggerWatchdogForTesting();
    bool TryBeginValidatedTransactionForTesting(const FThrowCommand& Command);
    void TriggerReleaseForTesting(const FTransform& GripWorldTransform);
#endif

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Character|Session 3")
    FDiscGolfRHBHAdapterReleaseEvent OnReleaseCommitted;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Character|Session 3")
    FDiscGolfRHBHAdapterRecoveryEvent OnThrowRecovered;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleFrameworkDiscRelease(FDGReleaseData ReleaseData);

    UFUNCTION()
    void HandleFrameworkThrowFinished();

    void BindFrameworkDelegates();
    void UnbindFrameworkDelegates();
    bool ArmWatchdog(uint64 AttemptSerial);
    void ClearWatchdog();
    void HandleWatchdog(uint64 ExpectedAttemptSerial);
    void CompleteRecovery(bool bDiscWasReleased);
    void SetHeldDiscVisible(bool bVisible) const;
    bool TryBeginValidatedTransaction(const FThrowCommand& Command);
    bool TryAcquireEquipmentMutationLock();
    void ReleaseEquipmentMutationLock();

    UPROPERTY(Transient)
    TObjectPtr<UDiscGolfThrowComponent> FrameworkThrowComponent;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> CharacterMesh;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> HeldDiscVisual;

    UPROPERTY(Transient)
    TObjectPtr<UThrowControllerComponent> ThrowController;

    UPROPERTY(Transient)
    TObjectPtr<UDiscBagComponent> DiscBag;

    UPROPERTY(EditAnywhere, Category="Disc Golf|Character|Session 3", meta=(ClampMin="0.25", ClampMax="60.0", UIMin="1.0", UIMax="10.0"))
    float WatchdogTimeoutSeconds = 5.0f;

    UPROPERTY(VisibleInstanceOnly, Category="Disc Golf|Character|Session 3")
    FThrowCommand LastAuthoritativeCommand;

    FDiscGolfRHBHThrowTransaction Transaction;
    FDiscGolfAuthoritativeRHBHLaunchDelegate AuthoritativeLaunchDelegate;
    FTimerHandle WatchdogTimer;
    bool bLastLaunchAccepted = false;
    bool bOwnsEquipmentMutationLock = false;
};
