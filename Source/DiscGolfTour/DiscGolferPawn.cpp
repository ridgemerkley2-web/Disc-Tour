#include "DiscGolferPawn.h"
#include "DiscBagComponent.h"
#include "DiscGolfInputConfig.h"
#include "DiscGolfTour.h"
#include "ThrowControllerComponent.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscGolferPresentationComponent.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfAppearanceComponent.h"
#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitComponent.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfAnimInstance.h"
#include "DiscGolfThrowComponent.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfSession5MocapValidationPaths.h"
#include "Algo/Unique.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    bool ResolveSession4ProfileOverride(
        UObject* Outer,
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutStyle,
        EDGHandedness& OutHandedness,
        FString& OutLabel)
    {
        if (!FApp::IsUnattended()
            || !FParse::Value(FCommandLine::Get(), TEXT("Session4Profile="), OutLabel))
        {
            return false;
        }

        const FString Normalized = OutLabel.ToLower();
        const TCHAR* PresetPath = nullptr;
        if (Normalized == TEXT("shortcompact") || Normalized == TEXT("short"))
        {
            PresetPath = TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact");
            OutLabel = TEXT("ShortCompact");
        }
        else if (Normalized == TEXT("baseline") || Normalized == TEXT("default"))
        {
            PresetPath = TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter");
            OutLabel = TEXT("Baseline");
        }
        else if (Normalized == TEXT("talllongarms") || Normalized == TEXT("tall"))
        {
            PresetPath = TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms");
            OutLabel = TEXT("TallLongArms");
        }

        if (PresetPath)
        {
            if (const UDiscGolfCharacterProfile* Preset = LoadObject<UDiscGolfCharacterProfile>(Outer, PresetPath))
            {
                OutBody = Preset->Body;
                OutStyle = Preset->ThrowStyle;
                OutHandedness = Preset->Handedness;
                return true;
            }
            return false;
        }

        if (Normalized == TEXT("slidermin") || Normalized == TEXT("minimum"))
        {
            FDiscGolfCharacterProfileSaveData Minimum;
            Minimum.HeightCm = DiscGolfCharacterCreatorSchema::MinHeightCm;
            Minimum.WingspanScale = DiscGolfCharacterCreatorSchema::MinWingspanScale;
            Minimum.ShoulderWidthScale = DiscGolfCharacterCreatorSchema::MinShoulderWidthScale;
            Minimum.TorsoLengthScale = DiscGolfCharacterCreatorSchema::MinTorsoLengthScale;
            Minimum.LegLengthScale = DiscGolfCharacterCreatorSchema::MinLegLengthScale;
            Minimum.HandScale = DiscGolfCharacterCreatorSchema::MinHandScale;
            Minimum.MassKg = DiscGolfCharacterCreatorSchema::MinMassKg;
            Minimum.RunUpIntensity = Minimum.ReachBackAmount = Minimum.TorsoRotation = 0.0f;
            Minimum.BraceIntensity = Minimum.Explosiveness = Minimum.FollowThrough = 0.0f;
            OutBody = Minimum.ToBodyProfile();
            OutStyle = Minimum.ToThrowStyle();
            OutHandedness = EDGHandedness::Right;
            OutLabel = TEXT("SliderMin");
            return true;
        }

        if (Normalized == TEXT("slidermax") || Normalized == TEXT("maximum"))
        {
            FDiscGolfCharacterProfileSaveData Maximum;
            Maximum.HeightCm = DiscGolfCharacterCreatorSchema::MaxHeightCm;
            Maximum.WingspanScale = DiscGolfCharacterCreatorSchema::MaxWingspanScale;
            Maximum.ShoulderWidthScale = DiscGolfCharacterCreatorSchema::MaxShoulderWidthScale;
            Maximum.TorsoLengthScale = DiscGolfCharacterCreatorSchema::MaxTorsoLengthScale;
            Maximum.LegLengthScale = DiscGolfCharacterCreatorSchema::MaxLegLengthScale;
            Maximum.HandScale = DiscGolfCharacterCreatorSchema::MaxHandScale;
            Maximum.MassKg = DiscGolfCharacterCreatorSchema::MaxMassKg;
            Maximum.RunUpIntensity = Maximum.ReachBackAmount = Maximum.TorsoRotation = 1.0f;
            Maximum.BraceIntensity = Maximum.Explosiveness = Maximum.FollowThrough = 1.0f;
            OutBody = Maximum.ToBodyProfile();
            OutStyle = Maximum.ToThrowStyle();
            OutHandedness = EDGHandedness::Right;
            OutLabel = TEXT("SliderMax");
            return true;
        }

        return false;
    }
}

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
    // Follow the profiled grip position/orientation without multiplying the
    // fixed gameplay-Cylinder dimensions by Control Rig socket scale.
    HeldDiscVisual->SetAbsolute(false, false, true);
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
    CharacterAppearance = CreateDefaultSubobject<UDiscGolfAppearanceComponent>(TEXT("CharacterAppearance"));
    OutfitComponent = CreateDefaultSubobject<UDiscGolfOutfitComponent>(TEXT("CharacterOutfit"));
    if (DefaultCharacterProfile.Succeeded())
    {
        CharacterProfileTemplate = DefaultCharacterProfile.Object;
        FrameworkThrowComponent->CharacterProfile = CharacterProfileTemplate;
    }
    RHBHThrowAdapter = CreateDefaultSubobject<UDiscGolfRHBHThrowAdapterComponent>(TEXT("RHBHThrowAdapter"));
    RHBHThrowMontage = RHBHMontage.Succeeded() ? RHBHMontage.Object : nullptr;
}

void ADiscGolferPawn::BeginPlay()
{
    Super::BeginPlay();

    // Session 5 may exercise a pipeline-produced montage only in an explicit,
    // unattended validation process.  The constructor and every normal game
    // launch retain the accepted Session 3 prototype montage.
    if (DiscGolfSession5MocapValidation::IsPipelineRuntimeValidationRequested())
    {
        RHBHThrowMontage = LoadObject<UAnimMontage>(
            nullptr,
            DiscGolfSession5MocapValidation::PipelineTestMontage);
        bSession5PipelineValidationMontageActive = RHBHThrowMontage != nullptr;
        if (bSession5PipelineValidationMontageActive)
        {
            UE_LOG(LogDiscGolfTour, Display,
                TEXT("SESSION 5 MOCAP VALIDATION MONTAGE OVERRIDE: %s (unattended test only; default gameplay unchanged)."),
                *RHBHThrowMontage->GetPathName());
        }
        else
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("SESSION 5 MOCAP VALIDATION MONTAGE OVERRIDE FAILED: %s"),
                DiscGolfSession5MocapValidation::PipelineTestMontage);
        }
    }

    if (CharacterProfileTemplate && FrameworkThrowComponent)
    {
        RuntimeCharacterProfile = DuplicateObject<UDiscGolfCharacterProfile>(
            CharacterProfileTemplate, this, TEXT("RuntimeCharacterProfile"));
        if (RuntimeCharacterProfile)
        {
            RuntimeCharacterProfile->SetFlags(RF_Transient);
            FrameworkThrowComponent->CharacterProfile = RuntimeCharacterProfile;

            FDGBodyProfile SavedBody = RuntimeCharacterProfile->Body;
            FDGThrowStyle SavedStyle = RuntimeCharacterProfile->ThrowStyle;
            EDGHandedness SavedHandedness = RuntimeCharacterProfile->Handedness;
            if (const UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance()))
            {
                Instance->GetCharacterProfile(SavedBody, SavedStyle, SavedHandedness);
            }
            FString Session4OverrideLabel;
            if (ResolveSession4ProfileOverride(
                    this, SavedBody, SavedStyle, SavedHandedness, Session4OverrideLabel))
            {
                UE_LOG(LogDiscGolfTour, Display,
                    TEXT("SESSION 4 PROFILE OVERRIDE: %s (transient, save slot unchanged)."),
                    *Session4OverrideLabel);
            }
            ApplyCharacterProfileUnchecked(SavedBody, SavedStyle, SavedHandedness);
        }
    }

    const bool bHasSkeletalAsset = SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset() != nullptr;
    if (SkeletalMesh) SkeletalMesh->SetVisibility(bHasSkeletalAsset);
    if (BodyMesh) BodyMesh->SetVisibility(!bHasSkeletalAsset);
    if (HeadMesh) HeadMesh->SetVisibility(!bHasSkeletalAsset);
    if (PresentationComponent) PresentationComponent->SetSkeletalAssetsReady(bHasSkeletalAsset);
    if (OutfitComponent)
    {
        OutfitComponent->Catalog = LoadObject<UDiscGolfOutfitCatalog>(
            nullptr, DiscGolfOutfitRuntime::CatalogObjectPath);
        OutfitComponent->OnBodyCoverageChanged.AddUniqueDynamic(
            this, &ADiscGolferPawn::HandleOutfitCoverageChanged);

        FDGOutfitLoadout SavedLoadout;
        if (const UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance()))
        {
            SavedLoadout = Instance->GetOutfitLoadout();
        }
        FString OutfitStatus;
        if (!ApplyOutfitLoadoutTransactionally(SavedLoadout, true, OutfitStatus)
            || !OutfitStatus.IsEmpty())
        {
            UE_LOG(LogDiscGolfTour, Warning, TEXT("Outfit startup recovery: %s"), *OutfitStatus);
        }
    }
    if (RHBHThrowAdapter)
    {
        RHBHThrowAdapter->Configure(FrameworkThrowComponent, SkeletalMesh, HeldDiscVisual);
        RHBHThrowAdapter->GetAuthoritativeLaunchDelegate().BindUObject(
            this, &ADiscGolferPawn::HandleAnimatedRHBHRelease);
        RHBHThrowAdapter->OnThrowRecovered.AddUniqueDynamic(
            this, &ADiscGolferPawn::HandleAnimatedThrowRecovered);
    }
}

bool ADiscGolferPawn::GetCharacterCreatorProfile(
    FDGBodyProfile& OutBody,
    FDGThrowStyle& OutStyle,
    EDGHandedness& OutHandedness) const
{
    const UDiscGolfCharacterProfile* Profile = RuntimeCharacterProfile
        ? RuntimeCharacterProfile.Get()
        : (FrameworkThrowComponent ? FrameworkThrowComponent->CharacterProfile.Get() : nullptr);
    if (!Profile)
    {
        return false;
    }

    OutBody = Profile->Body;
    OutStyle = Profile->ThrowStyle;
    OutHandedness = Profile->Handedness;
    return true;
}

bool ADiscGolferPawn::IsCharacterProfileChangeSafe() const
{
    if (IsAnimatedThrowActive() || (ThrowController && ThrowController->IsTimingActive()))
    {
        return false;
    }

    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    return !GameMode || GameMode->CanOpenCharacterCreator();
}

bool ADiscGolferPawn::PreviewCharacterCreatorProfile(
    const FDGBodyProfile& Body,
    const FDGThrowStyle& Style,
    EDGHandedness Handedness)
{
    if (!RuntimeCharacterProfile || !IsCharacterProfileChangeSafe())
    {
        return false;
    }

    ApplyCharacterProfileUnchecked(Body, Style, Handedness);
    return true;
}

void ADiscGolferPawn::ApplyCharacterProfileUnchecked(
    const FDGBodyProfile& Body,
    const FDGThrowStyle& Style,
    EDGHandedness Handedness)
{
    if (!RuntimeCharacterProfile)
    {
        return;
    }

    FDiscGolfCharacterProfileSaveData Safe =
        FDiscGolfCharacterProfileSaveData::FromFramework(Body, Style, Handedness);
    Safe.Sanitize();
    RuntimeCharacterProfile->Body = Safe.ToBodyProfile();
    RuntimeCharacterProfile->ThrowStyle = Safe.ToThrowStyle();
    RuntimeCharacterProfile->Handedness = Safe.GetHandedness();

    if (FrameworkThrowComponent)
    {
        FrameworkThrowComponent->CharacterProfile = RuntimeCharacterProfile;
    }
    if (CharacterAppearance)
    {
        CharacterAppearance->ApplyStandardMorphs(SkeletalMesh, RuntimeCharacterProfile->Body);
    }
    RefreshOutfitForCurrentBodyProfile();
    RefreshCharacterProfilePresentation();
}

UDiscGolfOutfitCatalog* ADiscGolferPawn::GetOutfitCatalog() const
{
    return OutfitComponent ? OutfitComponent->Catalog.Get() : nullptr;
}

const FDGOutfitLoadout& ADiscGolferPawn::GetCurrentOutfitLoadout() const
{
    static const FDGOutfitLoadout EmptyLoadout;
    return OutfitComponent ? OutfitComponent->CurrentLoadout : EmptyLoadout;
}

bool ADiscGolferPawn::ApplyOutfitLoadoutTransactionally(
    const FDGOutfitLoadout& Requested,
    bool bAllowUnavailableItems,
    FString& OutStatus)
{
    OutStatus.Reset();
    if (!OutfitComponent || !SkeletalMesh || !RuntimeCharacterProfile)
    {
        OutStatus = TEXT("Outfit runtime is unavailable; the current appearance was retained.");
        return false;
    }

    const FDGOutfitLoadout Previous = OutfitComponent->CurrentLoadout;
    const FDiscGolfOutfitResolution Resolution = DiscGolfOutfitRuntime::ResolveCanonicalLoadout(
        Requested, OutfitComponent->Catalog, RuntimeCharacterProfile->Body);
    if (!Resolution.bAllEntriesResolved && !bAllowUnavailableItems)
    {
        OutStatus = FString::Join(Resolution.Warnings, TEXT(" "));
        return false;
    }

    bool bApplied = false;
    if (Resolution.Loadout.Equipped.IsEmpty())
    {
        OutfitComponent->ClearOutfit();
        bApplied = true;
    }
    else
    {
        bApplied = OutfitComponent->ApplyLoadout(
            Resolution.Loadout, SkeletalMesh, RuntimeCharacterProfile->Body);
    }

    if (!bApplied || !DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            OutfitComponent->CurrentLoadout, Resolution.Loadout))
    {
        if (Previous.Equipped.IsEmpty())
        {
            OutfitComponent->ClearOutfit();
        }
        else
        {
            OutfitComponent->ApplyLoadout(
                Previous, SkeletalMesh, RuntimeCharacterProfile->Body);
        }
        HandleOutfitCoverageChanged(OutfitComponent->GetCoveredBodyRegions());
        OutStatus = TEXT("Outfit assets could not be applied; the previous loadout was restored.");
        return false;
    }

    RefreshOutfitForCurrentBodyProfile();
    if (!Resolution.Warnings.IsEmpty())
    {
        OutStatus = FString::Join(Resolution.Warnings, TEXT(" "));
    }
    return true;
}

void ADiscGolferPawn::RefreshOutfitForCurrentBodyProfile()
{
    if (!OutfitComponent || !RuntimeCharacterProfile)
    {
        return;
    }
    OutfitComponent->ReapplyBodyMorphs(RuntimeCharacterProfile->Body);
    HandleOutfitCoverageChanged(OutfitComponent->GetCoveredBodyRegions());
}

void ADiscGolferPawn::HandleOutfitCoverageChanged(
    const TArray<EDGBodyRegion>& CoveredRegions)
{
    CoveredOutfitBodyRegions = CoveredRegions;
    CoveredOutfitBodyRegions.Sort([](EDGBodyRegion A, EDGBodyRegion B)
    {
        return static_cast<uint8>(A) < static_cast<uint8>(B);
    });
    CoveredOutfitBodyRegions.SetNum(Algo::Unique(CoveredOutfitBodyRegions));
}

void ADiscGolferPawn::RefreshCharacterProfilePresentation()
{
    if (!SkeletalMesh || !RuntimeCharacterProfile)
    {
        return;
    }

    // NativeUpdateAnimation normally performs this copy. Push it explicitly as
    // well so a paused character-creator preview responds without a permanent
    // actor/component Tick.
    if (UDiscGolfAnimInstance* AnimInstance = Cast<UDiscGolfAnimInstance>(SkeletalMesh->GetAnimInstance()))
    {
        AnimInstance->BodyProfile = RuntimeCharacterProfile->Body;
        AnimInstance->ThrowStyle = RuntimeCharacterProfile->ThrowStyle;
        AnimInstance->Handedness = RuntimeCharacterProfile->Handedness;
    }
    SkeletalMesh->TickAnimation(0.0f, false);
    SkeletalMesh->RefreshBoneTransforms();
    SkeletalMesh->MarkRenderDynamicDataDirty();
}

void ADiscGolferPawn::BeginCharacterCreatorPreview()
{
    if (bCharacterCreatorPreviewActive || !CameraBoom || !Camera || !SkeletalMesh)
    {
        return;
    }

    bCharacterCreatorPreviewActive = true;
    SavedPreviewCameraArmLength = CameraBoom->TargetArmLength;
    SavedPreviewCameraSocketOffset = CameraBoom->SocketOffset;
    SavedPreviewCameraBoomRotation = CameraBoom->GetRelativeRotation();
    SavedPreviewCameraFov = Camera->FieldOfView;
    SavedPreviewSkeletalRotation = SkeletalMesh->GetRelativeRotation();
    bSavedSkeletalTickWhenPaused = SkeletalMesh->PrimaryComponentTick.bTickEvenWhenPaused;
    bSavedCameraBoomTickWhenPaused = CameraBoom->PrimaryComponentTick.bTickEvenWhenPaused;
    APlayerCameraManager* PlayerCameraManager = nullptr;
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        PlayerCameraManager = PlayerController->PlayerCameraManager;
    }
    if (PlayerCameraManager)
    {
        bSavedPlayerCameraManagerTickWhenPaused =
            PlayerCameraManager->PrimaryActorTick.bTickEvenWhenPaused;
        PlayerCameraManager->PrimaryActorTick.bTickEvenWhenPaused = true;
    }

    CameraBoom->PrimaryComponentTick.bTickEvenWhenPaused = true;
    CameraBoom->TargetArmLength = 430.0f;
    CameraBoom->SocketOffset = FVector(0.0f, -120.0f, 70.0f);
    CameraBoom->SetRelativeRotation(FRotator(-4.0f, 180.0f, 0.0f));
    Camera->SetFieldOfView(46.0f);
    SkeletalMesh->PrimaryComponentTick.bTickEvenWhenPaused = true;
    RefreshCharacterProfilePresentation();
    if (CameraBoom->IsRegistered())
    {
        // UE 5.8 caches the spring endpoint until TickComponent calls
        // UpdateDesiredArmLocation and propagates the socket to its children.
        CameraBoom->TickComponent(
            0.0f, ELevelTick::LEVELTICK_All, nullptr);
    }
    if (PlayerCameraManager)
    {
        // The creator opens after the controls menu pauses the world. The boom
        // and camera manager now both tick while paused; push the initial view
        // immediately as well so the first creator frame is deterministic.
        PlayerCameraManager->UpdateCamera(0.0f);
    }
}

void ADiscGolferPawn::EndCharacterCreatorPreview(bool bRestoreView)
{
    if (!bCharacterCreatorPreviewActive)
    {
        return;
    }

    bCharacterCreatorPreviewActive = false;
    if (SkeletalMesh)
    {
        SkeletalMesh->PrimaryComponentTick.bTickEvenWhenPaused = bSavedSkeletalTickWhenPaused;
        if (bRestoreView)
        {
            SkeletalMesh->SetRelativeRotation(SavedPreviewSkeletalRotation);
        }
    }
    if (bRestoreView && CameraBoom && Camera)
    {
        CameraBoom->TargetArmLength = SavedPreviewCameraArmLength;
        CameraBoom->SocketOffset = SavedPreviewCameraSocketOffset;
        CameraBoom->SetRelativeRotation(SavedPreviewCameraBoomRotation);
        Camera->SetFieldOfView(SavedPreviewCameraFov);
    }
    if (CameraBoom)
    {
        CameraBoom->PrimaryComponentTick.bTickEvenWhenPaused = bSavedCameraBoomTickWhenPaused;
    }
    if (bRestoreView && CameraBoom && CameraBoom->IsRegistered())
    {
        // Rebuild the restored spring endpoint before PlayerCameraManager
        // samples it, even though the controls menu still has world time paused.
        CameraBoom->TickComponent(
            0.0f, ELevelTick::LEVELTICK_All, nullptr);
    }
    APlayerCameraManager* PlayerCameraManager = nullptr;
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        PlayerCameraManager = PlayerController->PlayerCameraManager;
    }
    if (bRestoreView && PlayerCameraManager)
    {
        // Cancel/Apply also closes while paused, so restore the camera POV
        // before gameplay input and world time resume.
        PlayerCameraManager->UpdateCamera(0.0f);
    }
    if (PlayerCameraManager)
    {
        PlayerCameraManager->PrimaryActorTick.bTickEvenWhenPaused =
            bSavedPlayerCameraManagerTickWhenPaused;
    }
}

void ADiscGolferPawn::RotateCharacterCreatorPreview(float DeltaYawDegrees)
{
    if (!bCharacterCreatorPreviewActive || !SkeletalMesh || !FMath::IsFinite(DeltaYawDegrees))
    {
        return;
    }

    FRotator Rotation = SkeletalMesh->GetRelativeRotation();
    Rotation.Yaw = FMath::UnwindDegrees(Rotation.Yaw + FMath::Clamp(DeltaYawDegrees, -45.0f, 45.0f));
    SkeletalMesh->SetRelativeRotation(Rotation);
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

FString ADiscGolferPawn::GetActiveRHBHThrowMontagePath() const
{
    return RHBHThrowMontage ? RHBHThrowMontage->GetPathName() : FString();
}

bool ADiscGolferPawn::TryStartAnimatedRHBHThrow(const FThrowCommand& AuthoritativeCommand)
{
    if (!RHBHThrowAdapter || !RHBHThrowMontage || !SkeletalMesh)
    {
        return false;
    }

    // Session 4 persists handedness but deliberately does not fabricate a
    // mirrored LHBH montage. Left-handed players retain the legacy synchronous
    // gameplay path until a separately authored animation is accepted.
    if (FrameworkThrowComponent && FrameworkThrowComponent->CharacterProfile
        && FrameworkThrowComponent->CharacterProfile->Handedness != EDGHandedness::Right)
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
