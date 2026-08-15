#include "DiscGolferPawn.h"
#include "DiscBagComponent.h"
#include "DiscGolfInputConfig.h"
#include "DiscGolfTour.h"
#include "ThrowControllerComponent.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscGolferPresentationComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ADiscGolferPawn::ADiscGolferPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
    Capsule->InitCapsuleSize(34.0f, 88.0f);
    Capsule->SetCollisionProfileName(TEXT("Pawn"));
    SetRootComponent(Capsule);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(Capsule);
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyMesh->SetRelativeLocation(FVector(0, 0, -8));
    BodyMesh->SetRelativeScale3D(FVector(0.44f, 0.34f, 1.35f));
    if (CylinderMesh.Succeeded()) BodyMesh->SetStaticMesh(CylinderMesh.Object);

    HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
    HeadMesh->SetupAttachment(Capsule);
    HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadMesh->SetRelativeLocation(FVector(0, 0, 72));
    HeadMesh->SetRelativeScale3D(FVector(0.28f));
    if (SphereMesh.Succeeded()) HeadMesh->SetStaticMesh(SphereMesh.Object);

    SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalGolferMesh"));
    SkeletalMesh->SetupAttachment(Capsule);
    SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    SkeletalMesh->SetVisibility(false);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(Capsule);
    CameraBoom->TargetArmLength = 520.0f;
    CameraBoom->SocketOffset = FVector(0, 70, 80);
    CameraBoom->SetRelativeRotation(FRotator(-12.0f, 0, 0));
    CameraBoom->bDoCollisionTest = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraBoom);

    DiscBag = CreateDefaultSubobject<UDiscBagComponent>(TEXT("DiscBag"));
    ThrowController = CreateDefaultSubobject<UThrowControllerComponent>(TEXT("ThrowController"));
    PresentationComponent = CreateDefaultSubobject<UDiscGolferPresentationComponent>(TEXT("GolferPresentation"));
}

void ADiscGolferPawn::BeginPlay()
{
    Super::BeginPlay();
    const bool bHasSkeletalAsset = SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset() != nullptr;
    if (SkeletalMesh) SkeletalMesh->SetVisibility(bHasSkeletalAsset);
    if (BodyMesh) BodyMesh->SetVisibility(!bHasSkeletalAsset);
    if (HeadMesh) HeadMesh->SetVisibility(!bHasSkeletalAsset);
    if (PresentationComponent) PresentationComponent->SetSkeletalAssetsReady(bHasSkeletalAsset);
}

void ADiscGolferPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    ADiscGolfTourPlayerController* PlayerController = Cast<ADiscGolfTourPlayerController>(GetController());
    const UDiscGolfInputConfig* InputConfig = PlayerController ? PlayerController->EnsureGameplayInputReady() : nullptr;
    if (!EnhancedInput || !InputConfig)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc golfer input setup failed (EnhancedInputComponent=%s, InputConfig=%s)."),
            EnhancedInput ? TEXT("valid") : TEXT("missing"),
            InputConfig ? TEXT("valid") : TEXT("missing"));
        return;
    }

    EnhancedInput->BindAction(InputConfig->AimAction, ETriggerEvent::Triggered, this, &ADiscGolferPawn::InputAim);
    EnhancedInput->BindAction(InputConfig->PowerAction, ETriggerEvent::Triggered, this, &ADiscGolferPawn::InputPower);
    EnhancedInput->BindAction(InputConfig->HyzerAction, ETriggerEvent::Triggered, this, &ADiscGolferPawn::InputHyzer);
    EnhancedInput->BindAction(InputConfig->NoseAction, ETriggerEvent::Triggered, this, &ADiscGolferPawn::InputNose);

    EnhancedInput->BindAction(InputConfig->ThrowAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputThrow);
    EnhancedInput->BindAction(InputConfig->ToggleThrowStyleAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputToggleThrowStyle);
    EnhancedInput->BindAction(InputConfig->ResetHoleAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputResetHole);
    EnhancedInput->BindAction(InputConfig->CycleRegressionPresetAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputCycleRegressionPreset);
    EnhancedInput->BindAction(InputConfig->RunRegressionPresetAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputRunRegressionPreset);
    EnhancedInput->BindAction(InputConfig->RunRegressionSuiteAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputRunRegressionSuite);
    EnhancedInput->BindAction(InputConfig->ToggleShotTracerAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputToggleShotTracer);
    EnhancedInput->BindAction(InputConfig->InstantReplayAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputInstantReplay);
    EnhancedInput->BindAction(InputConfig->ToggleCourseAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputToggleCourse);
    EnhancedInput->BindAction(InputConfig->CourseFlyoverAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputCourseFlyover);
    EnhancedInput->BindAction(InputConfig->NextHoleAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputNextHole);
    EnhancedInput->BindAction(InputConfig->ScorecardAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputScorecard);
    EnhancedInput->BindAction(InputConfig->CyclePlasticAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputCyclePlastic);
    EnhancedInput->BindAction(InputConfig->Disc1Action, ETriggerEvent::Started, this, &ADiscGolferPawn::InputDisc1);
    EnhancedInput->BindAction(InputConfig->Disc2Action, ETriggerEvent::Started, this, &ADiscGolferPawn::InputDisc2);
    EnhancedInput->BindAction(InputConfig->Disc3Action, ETriggerEvent::Started, this, &ADiscGolferPawn::InputDisc3);
    EnhancedInput->BindAction(InputConfig->Disc4Action, ETriggerEvent::Started, this, &ADiscGolferPawn::InputDisc4);
    EnhancedInput->BindAction(InputConfig->Disc5Action, ETriggerEvent::Started, this, &ADiscGolferPawn::InputDisc5);
}

void ADiscGolferPawn::FaceLocation(const FVector& WorldLocation)
{
    const FVector ToTarget = WorldLocation - GetActorLocation();
    if (!ToTarget.IsNearlyZero())
    {
        const FRotator LookRotation = ToTarget.Rotation();
        SetActorRotation(FRotator(0.0f, LookRotation.Yaw, 0.0f));
    }
}

void ADiscGolferPawn::SetPresentationShotContext(
    EDiscShotContext ShotContext,
    float DistanceToBasketMeters)
{
    if (PresentationComponent) PresentationComponent->SetShotContext(ShotContext, DistanceToBasketMeters);
}

void ADiscGolferPawn::NotifyAuthoritativeRelease(const FThrowRelease& Release)
{
    if (PresentationComponent) PresentationComponent->CommitAuthoritativeRelease(Release);
}

void ADiscGolferPawn::CancelThrowPresentation()
{
    if (ThrowController) ThrowController->CancelTiming();
    if (PresentationComponent) PresentationComponent->CancelTiming();
}

FString ADiscGolferPawn::GetGolferPresentationStatusText() const
{
    return PresentationComponent ? PresentationComponent->GetStatusText() : TEXT("PRESENTATION UNAVAILABLE");
}

void ADiscGolferPawn::InputAim(const FInputActionValue& ActionValue)
{
    const float Value = ActionValue.Get<float>();
    if (FMath::IsNearlyZero(Value) || ThrowController->IsTimingActive()) return;
    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    const float Sensitivity = GameMode
        ? GameMode->GetPlayerSettings().ControllerSensitivity : 1.0f;
    AddActorWorldRotation(FRotator(0, Value * GetWorld()->GetDeltaSeconds() * 52.0f * Sensitivity, 0));
}

void ADiscGolferPawn::InputPower(const FInputActionValue& ActionValue)
{
    ThrowController->AdjustPower(ActionValue.Get<float>(), GetWorld()->GetDeltaSeconds());
}

void ADiscGolferPawn::InputHyzer(const FInputActionValue& ActionValue)
{
    ThrowController->AdjustHyzer(ActionValue.Get<float>(), GetWorld()->GetDeltaSeconds());
}

void ADiscGolferPawn::InputNose(const FInputActionValue& ActionValue)
{
    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    const float Invert = GameMode && GameMode->GetPlayerSettings().bInvertY ? -1.0f : 1.0f;
    ThrowController->AdjustNose(ActionValue.Get<float>() * Invert, GetWorld()->GetDeltaSeconds());
}

void ADiscGolferPawn::InputThrow()
{
    ADiscGolfTourGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    if (!GM || !GM->CanPlayerThrow())
    {
        CancelThrowPresentation();
        return;
    }

    FThrowCommand Command;
    if (ThrowController->HandleThrowPress(DiscBag->GetSelectedMoldId(), DiscBag->GetSelectedPlastic(), GetActorForwardVector(), Command))
    {
        GM->RequestThrow(Command);
    }
    else if (PresentationComponent)
    {
        PresentationComponent->BeginTiming(ThrowController->GetThrowStyle());
    }
}

void ADiscGolferPawn::InputToggleThrowStyle()
{
    ThrowController->ToggleThrowStyle();
}

void ADiscGolferPawn::InputResetHole()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>())
    {
        GM->ResetHole();
    }
}

void ADiscGolferPawn::InputCyclePlastic() { DiscBag->CyclePlastic(); }
void ADiscGolferPawn::InputCycleRegressionPreset()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->CyclePhysicsRegressionPreset();
}
void ADiscGolferPawn::InputRunRegressionPreset()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->RunSelectedPhysicsRegression();
}
void ADiscGolferPawn::InputRunRegressionSuite()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->RunPhysicsRegressionSuite();
}
void ADiscGolferPawn::InputToggleShotTracer()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleShotTracer();
}
void ADiscGolferPawn::InputInstantReplay()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleInstantReplay();
}
void ADiscGolferPawn::InputToggleCourse()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleCourse();
}
void ADiscGolferPawn::InputCourseFlyover()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->PreviewCourseFlyover();
}
void ADiscGolferPawn::InputNextHole()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->AdvanceToNextHole();
}
void ADiscGolferPawn::InputScorecard()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleScorecard();
}
void ADiscGolferPawn::InputDisc1() { DiscBag->SelectDiscIndex(0); }
void ADiscGolferPawn::InputDisc2() { DiscBag->SelectDiscIndex(1); }
void ADiscGolferPawn::InputDisc3() { DiscBag->SelectDiscIndex(2); }
void ADiscGolferPawn::InputDisc4() { DiscBag->SelectDiscIndex(3); }
void ADiscGolferPawn::InputDisc5() { DiscBag->SelectDiscIndex(4); }
