#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
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
    UFUNCTION(BlueprintPure) FString GetGolferPresentationStatusText() const;

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
