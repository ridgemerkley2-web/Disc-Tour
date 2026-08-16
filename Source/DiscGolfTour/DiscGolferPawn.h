#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DiscGolfCharacterTypes.h"
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
class UDiscGolfRHBHThrowAdapterComponent;
class UDiscGolfAppearanceComponent;
class UDiscGolfCharacterProfile;
class UAnimMontage;
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

    /** Session 4 works on a transient copy; preset PrimaryDataAssets stay immutable. */
    bool GetCharacterCreatorProfile(
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutStyle,
        EDGHandedness& OutHandedness) const;
    bool PreviewCharacterCreatorProfile(
        const FDGBodyProfile& Body,
        const FDGThrowStyle& Style,
        EDGHandedness Handedness);
    bool IsCharacterProfileChangeSafe() const;
    void BeginCharacterCreatorPreview();
    void EndCharacterCreatorPreview(bool bRestoreView = true);
    void RotateCharacterCreatorPreview(float DeltaYawDegrees);

    UDiscGolfCharacterProfile* GetRuntimeCharacterProfile() const { return RuntimeCharacterProfile; }
    USkeletalMeshComponent* GetSkeletalGolferMesh() const { return SkeletalMesh; }
    UStaticMeshComponent* GetHeldDiscVisual() const { return HeldDiscVisual; }

    /** Shared by real input and the end-to-end Session 3 smoke. */
    bool TryStartAnimatedRHBHThrow(const FThrowCommand& AuthoritativeCommand);
    bool CancelAnimatedThrowBeforeRelease();
    void CancelAnimatedThrow();

    void FaceLocation(const FVector& WorldLocation);
    void SetPresentationShotContext(EDiscShotContext ShotContext, float DistanceToBasketMeters);
    void NotifyAuthoritativeRelease(const FThrowRelease& Release);
    void CancelThrowPresentation();

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCapsuleComponent> Capsule;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BodyMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeadMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> SkeletalMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscBagComponent> DiscBag;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UThrowControllerComponent> ThrowController;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolferPresentationComponent> PresentationComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfThrowComponent> FrameworkThrowComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfAppearanceComponent> CharacterAppearance;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> RHBHThrowAdapter;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HeldDiscVisual;
    UPROPERTY() TObjectPtr<UAnimMontage> RHBHThrowMontage;
    UPROPERTY() TObjectPtr<UDiscGolfCharacterProfile> CharacterProfileTemplate;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfCharacterProfile> RuntimeCharacterProfile;

    bool bCharacterCreatorPreviewActive = false;
    bool bSavedSkeletalTickWhenPaused = false;
    bool bSavedCameraBoomTickWhenPaused = false;
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

    bool HandleAnimatedRHBHRelease(
        const FThrowCommand& AuthoritativeCommand,
        const FTransform& GripWorldTransform);
    void HandleRHBHMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    UFUNCTION()
    void HandleAnimatedThrowRecovered(int64 AttemptSerial, bool bDiscWasReleased);

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
