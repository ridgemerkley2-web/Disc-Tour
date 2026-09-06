#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAvatarBackendComponent.h"
#include "DiscGolfMetaHumanAvatarBackendComponent.generated.h"

class USkeletalMeshComponent;
class ULODSyncComponent;
class UDiscGolfMetaHumanOutfitRequiredBonesComponent;

/** Runtime-only presentation contexts for the fixed GameplayPerformance preset. */
UENUM(BlueprintType)
enum class EDGMetaHumanPresentationPolicy : uint8
{
    Unconfigured,
    CharacterCreator,
    GameplayPerformance,
};

/**
 * Project-side adapter for the fixed assembled MetaHuman presentation actor.
 * It links the runtime MetaHuman component contract; the actor must implement
 * the project visual contract and expose one tagged body and one tagged head.
 */
UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UDiscGolfMetaHumanAvatarBackendComponent
    : public UDiscGolfAvatarBackendComponent
{
    GENERATED_BODY()

public:
    /**
     * Selects the desired context for both the active visual and any later
     * candidate. An active transition is transactional and retains its last
     * verified policy when the requested policy cannot be proven.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Avatar|MetaHuman")
    bool SetPresentationPolicy(EDGMetaHumanPresentationPolicy Policy);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    EDGMetaHumanPresentationPolicy GetRequestedPresentationPolicy() const
    {
        CommitPendingPresentationPolicyEvidence();
        return RequestedPresentationPolicy;
    }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    EDGMetaHumanPresentationPolicy GetVerifiedPresentationPolicy() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    bool IsPresentationPolicyVerified() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    int32 GetVerifiedPresentationForcedLOD() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    FString GetPresentationPolicyStatus() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    int32 GetPresentationPolicyTransitionSuccessCount() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    int32 GetPresentationPolicyTransitionFailureCount() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    int32 GetPresentationPolicyRollbackSuccessCount() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    int32 GetPresentationPolicyRollbackFailureCount() const;

    /** Pure schema check used by runtime automation and live policy gates. */
    static bool ValidateGameplayPerformanceLODSyncContract(
        const ULODSyncComponent* LODSync,
        FString& OutStatus);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    USkeletalMeshComponent* GetVerifiedVisualBody() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    USkeletalMeshComponent* GetVerifiedVisualHead() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    USkeletalMeshComponent* GetVerifiedVisualOutfit() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    FString GetLastAdapterStatus() const { return LastAdapterStatus; }

    virtual bool ConfigureVisualBackend_Implementation(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const FDGFullCharacterCustomization& Customization) override;

    virtual bool ApplyVisualCustomization_Implementation(
        AActor* VisualActor,
        const FDGFullCharacterCustomization& Customization) override;

protected:
    virtual void OnRegister() override;

    virtual bool FinalizeVisualBackendActivation(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const FDGFullCharacterCustomization& Customization) override;

    virtual void OnUnregister() override;

private:
    UFUNCTION()
    void HandleAvatarBackendReady(FDGAvatarBackendState State);

    bool ApplyPresentationPolicyTransactionally(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const UDiscGolfAvatarBackendProfile* ExpectedProfile,
        EDGMetaHumanPresentationPolicy Policy,
        UDiscGolfMetaHumanOutfitRequiredBonesComponent* ExpectedHelper,
        USkeletalMeshComponent* ExpectedBody,
        USkeletalMeshComponent* ExpectedOutfit,
        bool bRequireLiveLODOutcomes,
        FString& OutStatus,
        bool* bOutMutationAttempted = nullptr,
        bool* bOutRollbackAttempted = nullptr,
        bool* bOutRollbackSucceeded = nullptr);

    void CommitPendingPresentationPolicyEvidence() const;

    bool ValidatePresentationPolicyState(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const UDiscGolfAvatarBackendProfile* ExpectedProfile,
        EDGMetaHumanPresentationPolicy Policy,
        bool bRequireLiveLODOutcomes,
        FString& OutStatus) const;

    bool InstallOutfitRequiredBonesHelper(
        AActor* VisualActor,
        USkeletalMeshComponent* Body,
        USkeletalMeshComponent* Outfit,
        UDiscGolfMetaHumanOutfitRequiredBonesComponent*& OutHelper,
        FString& OutStatus);

    bool RefreshAndVerifyOutfitRequiredBonesHelper(
        UDiscGolfMetaHumanOutfitRequiredBonesComponent* Helper,
        USkeletalMeshComponent* Body,
        USkeletalMeshComponent* Outfit,
        FString& OutStatus);

    void DestroyOutfitRequiredBonesHelper(
        UDiscGolfMetaHumanOutfitRequiredBonesComponent* Helper);

    void ResetOutfitRequiredBonesHelper();

    bool ValidateConfiguredVisual(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const UDiscGolfAvatarBackendProfile* ExpectedProfile,
        bool bRequireVerifiedRetarget,
        USkeletalMeshComponent*& OutBody,
        USkeletalMeshComponent*& OutHead,
        FString& OutStatus) const;

    UPROPERTY(Transient)
    TObjectPtr<AActor> VerifiedVisualActor;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> VerifiedVisualBody;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> VerifiedVisualHead;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> VerifiedVisualOutfit;

    UPROPERTY(Transient)
    TObjectPtr<UDiscGolfMetaHumanOutfitRequiredBonesComponent>
        OutfitRequiredBonesHelper;

    UPROPERTY(Transient)
    EDGMetaHumanPresentationPolicy RequestedPresentationPolicy =
        EDGMetaHumanPresentationPolicy::GameplayPerformance;

    UPROPERTY(Transient)
    EDGMetaHumanPresentationPolicy VerifiedPresentationPolicy =
        EDGMetaHumanPresentationPolicy::Unconfigured;

    mutable FString LastPresentationPolicyStatus =
        TEXT("GameplayPerformance policy is queued for the next verified visual.");

    mutable TWeakObjectPtr<AActor> PendingPresentationPolicyActor;
    mutable FString PendingPresentationPolicyStatus;
    mutable bool bPendingPresentationPolicyTransitionSuccess = false;
    mutable int32 PresentationPolicyTransitionSuccessCount = 0;
    mutable int32 PresentationPolicyTransitionFailureCount = 0;
    mutable int32 PresentationPolicyRollbackSuccessCount = 0;
    mutable int32 PresentationPolicyRollbackFailureCount = 0;

    UPROPERTY(Transient)
    FString LastAdapterStatus = TEXT("DG master proxy fallback active.");
};
