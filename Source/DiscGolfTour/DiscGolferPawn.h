#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfTypes.h"
#include "DiscGolferPawn.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UDiscBagComponent;
class UThrowControllerComponent;
class UDiscGolferPresentationComponent;
class UDiscGolfThrowComponent;
class UDiscGolfAnimationLibrary;
class UDiscGolfRHBHThrowAdapterComponent;
class UDiscGolfAppearanceComponent;
class UDiscGolfCharacterProfile;
class UDiscGolfCharacterCustomizationComponent;
class UDiscGolfMetaHumanAvatarBackendComponent;
class UDiscGolfCosmeticCatalog;
class UDiscGolfOutfitCatalog;
class UDiscGolfOutfitComponent;
class UAnimMontage;
enum class EGolferAnimationFamily : uint8;
struct FInputActionValue;
struct FThrowRelease;

UCLASS()
class DISCGOLFTOUR_API ADiscGolferPawn : public APawn
{
    GENERATED_BODY()

public:
    ADiscGolferPawn();
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintPure) UDiscBagComponent* GetDiscBag() const { return DiscBag; }
    UFUNCTION(BlueprintPure) UThrowControllerComponent* GetThrowController() const { return ThrowController; }
    UFUNCTION(BlueprintPure) UDiscGolferPresentationComponent* GetPresentationComponent() const { return PresentationComponent; }
    UFUNCTION(BlueprintPure) UDiscGolfRHBHThrowAdapterComponent* GetRHBHThrowAdapter() const { return RHBHThrowAdapter; }
    UFUNCTION(BlueprintPure) bool IsAnimatedThrowActive() const;
    UFUNCTION(BlueprintPure) FString GetGolferPresentationStatusText() const;

    /** Read-only evidence for command-line validation and runtime library selection. */
    FString GetActiveRHBHThrowMontagePath() const;
    /** Accepted runtime revision, or an isolated non-Shipping capture override. */
    const FString& GetProductionMotionCandidateRevision() const
    {
        return ProductionMotionCandidateRevision;
    }
    bool IsSession5PipelineValidationMontageActive() const
    {
        return bSession5PipelineValidationMontageActive;
    }

    /** Session 4 works on a transient copy; preset PrimaryDataAssets stay immutable. */
    bool GetCharacterCreatorProfile(
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutStyle,
        EDGHandedness& OutHandedness) const;
    bool PreviewCharacterCreatorProfile(
        const FDGBodyProfile& Body,
        const FDGThrowStyle& Style,
        EDGHandedness Handedness,
        bool bAllowAvatarBackendFallback = false);
    bool IsCharacterProfileChangeSafe() const;
    /** Enters creator presentation before mutating paused preview state. */
    bool BeginCharacterCreatorPreview();
    /** Restores gameplay presentation before releasing paused preview state. */
    bool EndCharacterCreatorPreview(bool bRestoreView = true);
    void RotateCharacterCreatorPreview(float DeltaYawDegrees);
    void ZoomCharacterCreatorPreview(float DeltaArmLength);
    /** Atomically frame the existing creator subject and refresh the paused view. */
    bool SetCharacterCreatorPreviewFraming(
        float TargetArmLength,
        float BoomPitchDegrees,
        float SubjectYawOffsetDegrees,
        float VerticalSocketOffsetCm);

    /** Complete Session 7 draft on the same possessed pawn. */
    FDGFullCharacterCustomization GetCurrentFullCharacterCustomization() const;
    bool PreviewFullCharacterCustomization(
        const FDGFullCharacterCustomization& Requested,
        FString& OutStatus);
    bool ApplyFullCharacterCustomizationTransactionally(
        const FDGFullCharacterCustomization& Requested,
        bool bAllowUnavailableItems,
        FString& OutStatus,
        bool bAllowAvatarBackendFallback = false);

    UDiscGolfCharacterProfile* GetRuntimeCharacterProfile() const { return RuntimeCharacterProfile; }
    USkeletalMeshComponent* GetSkeletalGolferMesh() const { return SkeletalMesh; }
    UStaticMeshComponent* GetHeldDiscVisual() const { return HeldDiscVisual; }
    UDiscGolfOutfitComponent* GetOutfitComponent() const { return OutfitComponent; }
    UDiscGolfCharacterCustomizationComponent* GetCharacterCustomizationComponent() const
    {
        return CharacterCustomization;
    }
    UDiscGolfMetaHumanAvatarBackendComponent* GetAvatarBackendComponent() const
    {
        return AvatarBackendComponent;
    }
    /** Read-only evidence seam; presentation policy remains private to the pawn. */
    bool IsDGProxyPresentationVisible() const;
    USkeletalMeshComponent* GetModularHeadMesh() const { return ModularHeadMesh; }
    UDiscGolfCosmeticCatalog* GetCosmeticCatalog() const;
    bool IsHairHiddenByOutfitCoverage() const { return bHairHiddenByOutfitCoverage; }
    UDiscGolfOutfitCatalog* GetOutfitCatalog() const;
    const FDGOutfitLoadout& GetCurrentOutfitLoadout() const;
    const TArray<EDGBodyRegion>& GetCoveredOutfitBodyRegions() const { return CoveredOutfitBodyRegions; }

    /** Canonical whole-loadout adapter; never owns animation, release, inventory, or flight. */
    bool ApplyOutfitLoadoutTransactionally(
        const FDGOutfitLoadout& Requested,
        bool bAllowUnavailableItems,
        FString& OutStatus);
    void RefreshOutfitForCurrentBodyProfile();

    /** Shared by real input and the end-to-end Session 3 smoke. */
    bool TryStartAnimatedRHBHThrow(const FThrowCommand& AuthoritativeCommand);
    bool CancelAnimatedThrowBeforeRelease();
    void CancelAnimatedThrow();

    void FaceLocation(const FVector& WorldLocation);
    void SetPresentationShotContext(EDiscShotContext ShotContext, float DistanceToBasketMeters);
    void NotifyAuthoritativeRelease(const FThrowRelease& Release);
    void CancelThrowPresentation();

    /** Shared by the bound Next Hole action and the Session 16 live-input smoke. */
    bool TryAdvanceToNextHoleFromPlayerInput();

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCapsuleComponent> Capsule;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BodyMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeadMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> SkeletalMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> ModularHeadMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscBagComponent> DiscBag;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UThrowControllerComponent> ThrowController;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolferPresentationComponent> PresentationComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfThrowComponent> FrameworkThrowComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfAppearanceComponent> CharacterAppearance;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfCharacterCustomizationComponent> CharacterCustomization;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfOutfitComponent> OutfitComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfMetaHumanAvatarBackendComponent> AvatarBackendComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> RHBHThrowAdapter;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeldDiscVisual;
    UPROPERTY() TObjectPtr<UDiscGolfAnimationLibrary> ProductionAnimationLibrary;
    UPROPERTY() TObjectPtr<UAnimMontage> RHBHThrowMontage;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveRHBHThrowMontage;
    UPROPERTY() TObjectPtr<UAnimMontage> ProductionDriveMontage;
    UPROPERTY() TObjectPtr<UAnimMontage> ProductionApproachMontage;
    UPROPERTY() TObjectPtr<UAnimMontage> ProductionPuttMontage;
    UPROPERTY() TObjectPtr<UDiscGolfCharacterProfile> CharacterProfileTemplate;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfCharacterProfile> RuntimeCharacterProfile;
    UPROPERTY(Transient) TArray<EDGBodyRegion> CoveredOutfitBodyRegions;

    bool bCharacterCreatorPreviewActive = false;
    bool bHairHiddenByOutfitCoverage = false;
    bool bSession5PipelineValidationMontageActive = false;
    FString ProductionMotionCandidateRevision;
    bool bDGProxyPresentationVisible = true;
    int64 ActiveRHBHMontageAttemptSerial = 0;
    bool bSavedSkeletalTickWhenPaused = false;
    bool bSavedCameraBoomTickWhenPaused = false;
    bool bSavedPlayerCameraManagerTickWhenPaused = false;
    float SavedPreviewCameraArmLength = 0.0f;
    float SavedPreviewCameraFov = 90.0f;
    FVector SavedPreviewCameraSocketOffset = FVector::ZeroVector;
    FRotator SavedPreviewCameraBoomRotation = FRotator::ZeroRotator;
    FRotator SavedPreviewSkeletalRotation = FRotator::ZeroRotator;

    void ApplyCharacterProfileUnchecked(
        const FDGBodyProfile& Body,
        const FDGThrowStyle& Style,
        EDGHandedness Handedness);
    void RefreshCharacterProfilePresentation();
    void ApplyFullCustomizationVisuals();
    void RebuildCustomizationHairForCoverage();
    void SetDGProxyPresentationVisible(bool bVisible);
    bool ApplyAvatarBackendForCustomization(
        const FDGFullCharacterCustomization& Customization,
        bool bAllowDGMasterFallback,
        FString& OutStatus);

    bool HandleAnimatedRHBHRelease(
        const FThrowCommand& AuthoritativeCommand,
        const FTransform& GripWorldTransform);
    void HandleRHBHMontageEnded(
        UAnimMontage* Montage,
        bool bInterrupted,
        int64 ExpectedAttemptSerial);
    void RefreshPreCommitThrowPresentation();
    void EndPreCommitThrowPresentation();
    UAnimMontage* ResolveRHBHThrowMontage(
        const FThrowCommand& AuthoritativeCommand,
        EGolferAnimationFamily AnimationFamily) const;
    void StopAndClearActiveRHBHMontage(float BlendOutSeconds);

    UFUNCTION()
    void HandleAnimatedThrowRecovered(int64 AttemptSerial, bool bDiscWasReleased);

    UFUNCTION()
    void HandleOutfitCoverageChanged(const TArray<EDGBodyRegion>& CoveredRegions);

    void InputAim(const FInputActionValue& Value);
    void InputPower(const FInputActionValue& Value);
    void InputHyzer(const FInputActionValue& Value);
    void InputNose(const FInputActionValue& Value);
    void InputThrow();
    void InputToggleThrowStyle();
    void InputResetHole();
    void InputCycleRegressionPreset();
    void InputRunRegressionPreset();
    void InputRunRegressionSuite();
    void InputToggleShotTracer();
    void InputInstantReplay();
    void InputToggleCourse();
    void InputCourseFlyover();
    void InputNextHole();
    void InputScorecard();
    void InputCyclePlastic();
    void InputDisc1();
    void InputDisc2();
    void InputDisc3();
    void InputDisc4();
    void InputDisc5();
};
