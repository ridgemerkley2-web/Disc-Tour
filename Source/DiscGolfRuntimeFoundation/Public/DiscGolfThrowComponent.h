#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfThrowComponent.generated.h"

class UDiscGolfCharacterProfile;
class UAnimMontage;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDGThrowPhaseChanged, EDGThrowPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDGDiscReleased, FDGReleaseData, ReleaseData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDGThrowFinished);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfThrowComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfThrowComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UDiscGolfCharacterProfile> CharacterProfile;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FDGThrowIntent CurrentIntent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDGThrowPhase CurrentPhase = EDGThrowPhase::Idle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bThrowActive = false;

    UPROPERTY(BlueprintAssignable) FDGThrowPhaseChanged OnThrowPhaseChanged;
    UPROPERTY(BlueprintAssignable) FDGDiscReleased OnDiscRelease;
    UPROPERTY(BlueprintAssignable) FDGThrowFinished OnThrowFinished;

    UFUNCTION(BlueprintCallable) void SetThrowIntent(const FDGThrowIntent& Intent);
    /** Starts the presentation-only aim/waggle state before a command is committed. */
    UFUNCTION(BlueprintCallable) void BeginAimPreview();
    /** Starts a committed authored throw. Gameplay release authority remains external. */
    UFUNCTION(BlueprintCallable) void BeginThrow();
    UFUNCTION(BlueprintCallable) void CancelThrow();
    UFUNCTION(BlueprintCallable) void NotifyThrowPhase(EDGThrowPhase Phase);
    UFUNCTION(BlueprintCallable) void NotifyDiscRelease(USkeletalMeshComponent* CharacterMesh);
    UFUNCTION(BlueprintCallable) void NotifyThrowFinished();
    /** Ends the recovery tail after the montage has naturally blended out. */
    UFUNCTION(BlueprintCallable) void NotifyRecoveryComplete();
    UFUNCTION(BlueprintPure) FName GetActiveDiscGripBone() const;
    UFUNCTION(BlueprintPure) bool IsThrowCommitted() const { return bThrowCommitted; }
    UFUNCTION(BlueprintPure) bool HasNotifiedDiscRelease() const { return bDiscReleaseNotified; }
    UFUNCTION(BlueprintPure) bool HasNotifiedThrowFinished() const { return bThrowFinishedNotified; }

    /** Binds engine notify forwarding to one validated authored montage instance. */
    bool BindCommittedMontageInstance(
        int64 AttemptSerial,
        int32 MontageInstanceId,
        UAnimMontage* Montage);
    void NotifyThrowPhaseFromMontage(
        EDGThrowPhase Phase,
        int32 MontageInstanceId,
        const UAnimMontage* SourceMontage);
    void NotifyDiscReleaseFromMontage(
        USkeletalMeshComponent* CharacterMesh,
        int32 MontageInstanceId,
        const UAnimMontage* SourceMontage);
    void NotifyThrowFinishedFromMontage(
        int32 MontageInstanceId,
        const UAnimMontage* SourceMontage);

    /** Read-only proof token for the exact authored montage instance owning this throw. */
    int32 GetCommittedMontageInstanceId() const { return ActiveMontageInstanceId; }
    bool IsCommittedMontageInstance(
        int32 MontageInstanceId,
        const UAnimMontage* SourceMontage) const
    {
        return IsExpectedMontageInstance(MontageInstanceId, SourceMontage);
    }

    /** Runtime fail-closed contract shared by selection and notify binding. */
    static bool IsAuthoredMontageLifecycleSafe(const UAnimMontage* Montage);

private:
    void ResetLifecycle();
    bool CanAcceptAuthoredPhase(EDGThrowPhase Phase) const;
    bool IsExpectedMontageInstance(
        int32 MontageInstanceId,
        const UAnimMontage* SourceMontage) const;
    bool ReconcileQueuedPhasesThrough(EDGThrowPhase TargetPhase);

    int64 ActivePresentationAttemptSerial = 0;
    int32 ActiveMontageInstanceId = INDEX_NONE;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveAuthoredMontage;
    bool bThrowCommitted = false;
    bool bDiscReleaseNotified = false;
    bool bThrowFinishedNotified = false;
};
