#include "DiscGolferPawn.h"
#include "DiscBagComponent.h"
#include "DiscGolfInputConfig.h"
#include "DiscGolfTour.h"
#include "ThrowControllerComponent.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscGolferPresentationComponent.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfThrowComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MasterGolferMesh(
        TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> PlayerAnimationBlueprint(
        TEXT("/Game/DiscGolf/Animation/ABP_DG_Player"));
    static ConstructorHelpers::FObjectFinder<UAnimMontage> RHBHMontage(
        TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype"));
    static ConstructorHelpers::FObjectFinder<UDiscGolfCharacterProfile> DefaultCharacterProfile(
        TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"));

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
    // The generated validation mesh faces +Y; the gameplay pawn faces +X.
    SkeletalMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    SkeletalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    SkeletalMesh->bEnableUpdateRateOptimizations = false;
    if (MasterGolferMesh.Succeeded()) SkeletalMesh->SetSkeletalMeshAsset(MasterGolferMesh.Object);
    if (PlayerAnimationBlueprint.Succeeded())
    {
        SkeletalMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        SkeletalMesh->SetAnimInstanceClass(PlayerAnimationBlueprint.Class);
    }
    SkeletalMesh->SetVisibility(false);

    HeldDiscVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldDiscVisual"));
    HeldDiscVisual->SetupAttachment(SkeletalMesh, TEXT("disc_grip_r"));
    HeldDiscVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeldDiscVisual->SetGenerateOverlapEvents(false);
    HeldDiscVisual->SetRelativeLocation(FVector::ZeroVector);
    HeldDiscVisual->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
    HeldDiscVisual->SetRelativeScale3D(FVector(0.21f, 0.21f, 0.015f));
    HeldDiscVisual->SetHiddenInGame(true);
    HeldDiscVisual->SetVisibility(false);
    if (CylinderMesh.Succeeded()) HeldDiscVisual->SetStaticMesh(CylinderMesh.Object);

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
    FrameworkThrowComponent = CreateDefaultSubobject<UDiscGolfThrowComponent>(TEXT("CharacterFrameworkThrow"));
    if (DefaultCharacterProfile.Succeeded())
    {
        FrameworkThrowComponent->CharacterProfile = DefaultCharacterProfile.Object;
    }
    RHBHThrowAdapter = CreateDefaultSubobject<UDiscGolfRHBHThrowAdapterComponent>(TEXT("RHBHThrowAdapter"));
    RHBHThrowMontage = RHBHMontage.Succeeded() ? RHBHMontage.Object : nullptr;
}

void ADiscGolferPawn::BeginPlay()
{
    Super::BeginPlay();
    const bool bHasSkeletalAsset = SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset() != nullptr;
    if (SkeletalMesh) SkeletalMesh->SetVisibility(bHasSkeletalAsset);
    if (BodyMesh) BodyMesh->SetVisibility(!bHasSkeletalAsset);
    if (HeadMesh) HeadMesh->SetVisibility(!bHasSkeletalAsset);
    if (PresentationComponent) PresentationComponent->SetSkeletalAssetsReady(bHasSkeletalAsset);
    if (RHBHThrowAdapter)
    {
        RHBHThrowAdapter->Configure(FrameworkThrowComponent, SkeletalMesh, HeldDiscVisual);
        RHBHThrowAdapter->GetAuthoritativeLaunchDelegate().BindUObject(
            this, &ADiscGolferPawn::HandleAnimatedRHBHRelease);
        RHBHThrowAdapter->OnThrowRecovered.AddUniqueDynamic(
            this, &ADiscGolferPawn::HandleAnimatedThrowRecovered);
    }
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

bool ADiscGolferPawn::IsAnimatedThrowActive() const
{
    return RHBHThrowAdapter && RHBHThrowAdapter->IsThrowActive();
}

bool ADiscGolferPawn::TryStartAnimatedRHBHThrow(const FThrowCommand& AuthoritativeCommand)
{
    if (!RHBHThrowAdapter || !RHBHThrowMontage || !SkeletalMesh)
    {
        return false;
    }

    UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
    if (!AnimInstance || !RHBHThrowAdapter->TryBeginRHBHThrow(AuthoritativeCommand))
    {
        return false;
    }

    const float PlayedDuration = AnimInstance->Montage_Play(RHBHThrowMontage, 1.0f);
    if (PlayedDuration <= 0.0f)
    {
        RHBHThrowAdapter->CancelBeforeRelease();
        return false;
    }

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &ADiscGolferPawn::HandleRHBHMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, RHBHThrowMontage);
    return true;
}

bool ADiscGolferPawn::CancelAnimatedThrowBeforeRelease()
{
    return RHBHThrowAdapter && RHBHThrowAdapter->CancelBeforeRelease();
}

void ADiscGolferPawn::CancelAnimatedThrow()
{
    if (!RHBHThrowAdapter || !RHBHThrowAdapter->IsThrowActive())
    {
        return;
    }

    if (RHBHThrowAdapter->IsAwaitingRelease())
    {
        RHBHThrowAdapter->CancelBeforeRelease();
    }
    else
    {
        RHBHThrowAdapter->RecoverInterruptedThrow();
    }
}

bool ADiscGolferPawn::HandleAnimatedRHBHRelease(
    const FThrowCommand& AuthoritativeCommand,
    const FTransform& GripWorldTransform)
{
    ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    return GameMode && GameMode->RequestThrowFromGrip(AuthoritativeCommand, GripWorldTransform);
}

void ADiscGolferPawn::HandleRHBHMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == RHBHThrowMontage && RHBHThrowAdapter && RHBHThrowAdapter->IsThrowActive())
    {
        RHBHThrowAdapter->RecoverInterruptedThrow();
    }
}

void ADiscGolferPawn::HandleAnimatedThrowRecovered(int64 AttemptSerial, bool bDiscWasReleased)
{
    (void)AttemptSerial;

    // A pre-release cancellation (or a release-frame handoff rejected by the
    // gameplay authority) must also clear the legacy presentation/timing state.
    // Successful launches retain the existing release/follow-through feedback.
    if (!bDiscWasReleased
        || !RHBHThrowAdapter
        || !RHBHThrowAdapter->WasLastAuthoritativeLaunchAccepted())
    {
        CancelThrowPresentation();
    }

    if (SkeletalMesh)
    {
        if (UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
            AnimInstance && RHBHThrowMontage && AnimInstance->Montage_IsPlaying(RHBHThrowMontage))
        {
            // Recovery must quiesce the old montage before a later throw begins;
            // framework notifies do not carry a transaction token.
            AnimInstance->Montage_Stop(0.05f, RHBHThrowMontage);
        }
    }
}

FString ADiscGolferPawn::GetGolferPresentationStatusText() const
{
    return PresentationComponent ? PresentationComponent->GetStatusText() : TEXT("PRESENTATION UNAVAILABLE");
}

void ADiscGolferPawn::InputAim(const FInputActionValue& ActionValue)
{
    const float Value = ActionValue.Get<float>();
    if (FMath::IsNearlyZero(Value) || ThrowController->IsTimingActive() || IsAnimatedThrowActive()) return;
    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    const float Sensitivity = GameMode
        ? GameMode->GetPlayerSettings().ControllerSensitivity : 1.0f;
    AddActorWorldRotation(FRotator(0, Value * GetWorld()->GetDeltaSeconds() * 52.0f * Sensitivity, 0));
}

void ADiscGolferPawn::InputPower(const FInputActionValue& ActionValue)
{
    if (IsAnimatedThrowActive()) return;
    ThrowController->AdjustPower(ActionValue.Get<float>(), GetWorld()->GetDeltaSeconds());
}

void ADiscGolferPawn::InputHyzer(const FInputActionValue& ActionValue)
{
    if (IsAnimatedThrowActive()) return;
    ThrowController->AdjustHyzer(ActionValue.Get<float>(), GetWorld()->GetDeltaSeconds());
}

void ADiscGolferPawn::InputNose(const FInputActionValue& ActionValue)
{
    if (IsAnimatedThrowActive()) return;
    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    const float Invert = GameMode && GameMode->GetPlayerSettings().bInvertY ? -1.0f : 1.0f;
    ThrowController->AdjustNose(ActionValue.Get<float>() * Invert, GetWorld()->GetDeltaSeconds());
}

void ADiscGolferPawn::InputThrow()
{
    if (IsAnimatedThrowActive()) return;
    ADiscGolfTourGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    if (!GM || !GM->CanPlayerThrow())
    {
        CancelThrowPresentation();
        return;
    }

    FThrowCommand Command;
    if (ThrowController->HandleThrowPress(DiscBag->GetSelectedMoldId(), DiscBag->GetSelectedPlastic(), GetActorForwardVector(), Command))
    {
        if (Command.ThrowStyle != EThrowStyle::Backhand
            || Command.ShotContext != EDiscShotContext::Drive
            || !TryStartAnimatedRHBHThrow(Command))
        {
            // Non-RHBH/putting behavior stays synchronous. A missing Session 3
            // presentation asset also degrades safely to the existing gameplay path.
            GM->RequestThrow(Command);
        }
    }
    else if (PresentationComponent)
    {
        PresentationComponent->BeginTiming(ThrowController->GetThrowStyle());
    }
}

void ADiscGolferPawn::InputToggleThrowStyle()
{
    if (IsAnimatedThrowActive()) return;
    ThrowController->ToggleThrowStyle();
}

void ADiscGolferPawn::InputResetHole()
{
    CancelAnimatedThrow();
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>())
    {
        GM->ResetHole();
    }
}

void ADiscGolferPawn::InputCyclePlastic() { if (!IsAnimatedThrowActive()) DiscBag->CyclePlastic(); }
void ADiscGolferPawn::InputCycleRegressionPreset()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->CyclePhysicsRegressionPreset();
}
void ADiscGolferPawn::InputRunRegressionPreset()
{
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->RunSelectedPhysicsRegression();
}
void ADiscGolferPawn::InputRunRegressionSuite()
{
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->RunPhysicsRegressionSuite();
}
void ADiscGolferPawn::InputToggleShotTracer()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleShotTracer();
}
void ADiscGolferPawn::InputInstantReplay()
{
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleInstantReplay();
}
void ADiscGolferPawn::InputToggleCourse()
{
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleCourse();
}
void ADiscGolferPawn::InputCourseFlyover()
{
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->PreviewCourseFlyover();
}
void ADiscGolferPawn::InputNextHole()
{
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->AdvanceToNextHole();
}
void ADiscGolferPawn::InputScorecard()
{
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleScorecard();
}
void ADiscGolferPawn::InputDisc1() { if (!IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(0); }
void ADiscGolferPawn::InputDisc2() { if (!IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(1); }
void ADiscGolferPawn::InputDisc3() { if (!IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(2); }
void ADiscGolferPawn::InputDisc4() { if (!IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(3); }
void ADiscGolferPawn::InputDisc5() { if (!IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(4); }
