#include "DiscGolferPawn.h"
#include "DiscBagComponent.h"
#include "DiscGolfInputConfig.h"
#include "DiscGolfInputRoutePolicy.h"
#include "DiscGolfHoleActor.h"
#include "DiscGolfMath.h"
#include "DiscGolfTour.h"
#include "ThrowControllerComponent.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscGolferPresentationComponent.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfAppearanceComponent.h"
#include "DiscGolfAvatarBackendProfile.h"
#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfCharacterCustomizationComponent.h"
#include "DiscGolfCosmeticCatalog.h"
#include "DiscGolfFullCharacterRuntime.h"
#include "DiscGolfMetaHumanAvatarBackendComponent.h"
#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitComponent.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfProductionMotion.h"
#include "DiscGolfAnimationLibrary.h"
#include "DiscGolfAnimInstance.h"
#include "DiscGolfThrowComponent.h"
#include "DiscGolfTourGameInstance.h"
#if DG_WITH_DEVELOPMENT_CONTENT
#include "DiscGolfSession5MocapValidationPaths.h"
#endif
#include "Algo/Unique.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "InputActionValue.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float MinimumThrowMontagePlayRate = 0.90f;
    constexpr float MaximumThrowMontagePlayRate = 1.10f;

    FName MotionFamilyId(EGolferAnimationFamily Family)
    {
        switch (Family)
        {
            case EGolferAnimationFamily::Approach: return TEXT("Approach");
            case EGolferAnimationFamily::Putt: return TEXT("Putt");
            default: return TEXT("Drive");
        }
    }

    bool IsRecommendedPowerRangeValid(const FDGThrowAnimationEntry& Entry)
    {
        return FMath::IsFinite(Entry.RecommendedPowerMin)
            && FMath::IsFinite(Entry.RecommendedPowerMax)
            && Entry.RecommendedPowerMin >= 0.0f
            && Entry.RecommendedPowerMax <= 1.0f
            && Entry.RecommendedPowerMin <= Entry.RecommendedPowerMax;
    }

    bool IsMontageLifecycleSafe(const UAnimMontage* Montage)
    {
        return UDiscGolfThrowComponent::IsAuthoredMontageLifecycleSafe(Montage);
    }

    float ComputeThrowMontagePlayRate(const FThrowCommand& Command)
    {
        if (!DiscGolfMath::IsThrowCommandValid(Command))
        {
            return 1.0f;
        }

        // Presentation cadence varies narrowly with captured command intent.
        // It never feeds back into release speed, spin, angle, or direction.
        const float PowerRate = FMath::Lerp(
            0.94f, 1.06f, FMath::Clamp(Command.Power01, 0.0f, 1.0f));
        const float TimingRate = FMath::Lerp(
            1.0f, 0.97f, FMath::Clamp(FMath::Abs(Command.TimingError), 0.0f, 1.0f));
        return FMath::Clamp(
            PowerRate * TimingRate,
            MinimumThrowMontagePlayRate,
            MaximumThrowMontagePlayRate);
    }

    bool IsRouteAllowed(const ADiscGolferPawn* Golfer, EDiscGolfInputRoute Route)
    {
        const ADiscGolfTourPlayerController* Controller = Golfer
            ? Cast<ADiscGolfTourPlayerController>(Golfer->GetController()) : nullptr;
        return !Controller || Controller->IsInputRouteAllowed(Route);
    }

    EDiscGolfInputRoute GetActiveInputRoute(const ADiscGolferPawn* Golfer)
    {
        const ADiscGolfTourPlayerController* Controller = Golfer
            ? Cast<ADiscGolfTourPlayerController>(Golfer->GetController()) : nullptr;
        return Controller ? Controller->GetActiveInputRoute() : EDiscGolfInputRoute::Gameplay;
    }

#if DG_WITH_DEVELOPMENT_CONTENT
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
#endif
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
#if DG_WITH_DEVELOPMENT_CONTENT
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> ModularProxyHead(
        DiscGolfFullCharacterRuntime::HeadMeshObjectPath);
#endif
    static ConstructorHelpers::FClassFinder<UAnimInstance> PlayerAnimationBlueprint(
        TEXT("/Game/DiscGolf/Animation/ABP_DG_Player"));
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

    ModularHeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(
        TEXT("ModularCharacterHead"));
    ModularHeadMesh->SetupAttachment(SkeletalMesh);
    ModularHeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ModularHeadMesh->SetGenerateOverlapEvents(false);
    ModularHeadMesh->SetCanEverAffectNavigation(false);
    ModularHeadMesh->SetSimulatePhysics(false);
    ModularHeadMesh->VisibilityBasedAnimTickOption =
        EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    ModularHeadMesh->bEnableUpdateRateOptimizations = false;
#if DG_WITH_DEVELOPMENT_CONTENT
    if (ModularProxyHead.Succeeded())
    {
        ModularHeadMesh->SetSkeletalMeshAsset(ModularProxyHead.Object);
    }
#endif
    ModularHeadMesh->SetVisibility(false);

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
    CharacterCustomization = CreateDefaultSubobject<UDiscGolfCharacterCustomizationComponent>(
        TEXT("CharacterCustomization"));
    OutfitComponent = CreateDefaultSubobject<UDiscGolfOutfitComponent>(TEXT("CharacterOutfit"));
    AvatarBackendComponent =
        CreateDefaultSubobject<UDiscGolfMetaHumanAvatarBackendComponent>(
            TEXT("MetaHumanVisualBackend"));
    if (DefaultCharacterProfile.Succeeded())
    {
        CharacterProfileTemplate = DefaultCharacterProfile.Object;
        FrameworkThrowComponent->CharacterProfile = CharacterProfileTemplate;
    }
    RHBHThrowAdapter = CreateDefaultSubobject<UDiscGolfRHBHThrowAdapterComponent>(TEXT("RHBHThrowAdapter"));
    ProductionAnimationLibrary = nullptr;
    ProductionDriveMontage = nullptr;
    ProductionApproachMontage = nullptr;
    ProductionPuttMontage = nullptr;
    RHBHThrowMontage = nullptr;
}

void ADiscGolferPawn::BeginPlay()
{
    Super::BeginPlay();

    // Runtime loading happens after CDO construction so the guarded authoring
    // commandlet can run before these candidate packages exist. Packaging still
    // includes the complete root through the explicit AlwaysCook contract.
    ProductionDriveMontage = LoadObject<UAnimMontage>(
        nullptr, DiscGolfProductionMotion::DriveMontage);
    ProductionApproachMontage = LoadObject<UAnimMontage>(
        nullptr, DiscGolfProductionMotion::ApproachMontage);
    ProductionPuttMontage = LoadObject<UAnimMontage>(
        nullptr, DiscGolfProductionMotion::PuttMontage);
    ProductionAnimationLibrary = LoadObject<UDiscGolfAnimationLibrary>(
        nullptr, DiscGolfProductionMotion::Library);
    RHBHThrowMontage = ProductionDriveMontage;
    ProductionMotionCandidateRevision =
        DiscGolfProductionMotion::ActiveAssetRevision;

#if DG_WITH_DEVELOPMENT_CONTENT
    // An explicitly requested evidence capture may exercise a staged candidate
    // without changing ActiveVersion, the accepted library asset, or Shipping.
    // Load into temporaries and swap all four pointers atomically only after the
    // exact v007 library and all three montages prove present and internally
    // bound to the expected versioned object paths.
    FString RequestedMotionCandidate;
    const bool bCandidateValuePresent = FParse::Value(
        FCommandLine::Get(), TEXT("DGProductionMotionCandidate="),
        RequestedMotionCandidate);
    if (bCandidateValuePresent)
    {
        const bool bIsVisualEvidenceCapture =
            FParse::Param(FCommandLine::Get(),
                TEXT("Session19MetaHumanProductionVisualCapture"))
            || FParse::Param(FCommandLine::Get(),
                TEXT("Session19ProductionMotionVisualCapture"));
        const bool bExactRequest = RequestedMotionCandidate == TEXT("v007")
            && bIsVisualEvidenceCapture
            && FParse::Param(FCommandLine::Get(), TEXT("unattended"));
        if (!bExactRequest)
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("PRODUCTION MOTION CANDIDATE OVERRIDE REFUSED: exact v007 + unattended Session19 visual capture is required (requested=%s)."),
                *RequestedMotionCandidate);
        }
        else
        {
            UAnimMontage* CandidateDrive = LoadObject<UAnimMontage>(
                nullptr, DiscGolfProductionMotion::V7::DriveMontage);
            UAnimMontage* CandidateApproach = LoadObject<UAnimMontage>(
                nullptr, DiscGolfProductionMotion::V7::ApproachMontage);
            UAnimMontage* CandidatePutt = LoadObject<UAnimMontage>(
                nullptr, DiscGolfProductionMotion::V7::PuttMontage);
            UDiscGolfAnimationLibrary* CandidateLibrary =
                LoadObject<UDiscGolfAnimationLibrary>(
                    nullptr, DiscGolfProductionMotion::V7::Library);
            const TMap<FName, FString> ExpectedMontages = {
                {TEXT("Drive"), DiscGolfProductionMotion::V7::DriveMontage},
                {TEXT("Approach"), DiscGolfProductionMotion::V7::ApproachMontage},
                {TEXT("Putt"), DiscGolfProductionMotion::V7::PuttMontage},
            };
            const TMap<FName, FName> ExpectedStyles = {
                {TEXT("Drive"), TEXT("DG_Drive_Procedural_Candidate_v007")},
                {TEXT("Approach"), TEXT("DG_Approach_Procedural_Candidate_v007")},
                {TEXT("Putt"), TEXT("DG_Putt_Procedural_Candidate_v007")},
            };
            const TMap<FName, FVector2D> ExpectedPowerRanges = {
                {TEXT("Drive"), FVector2D(0.62, 1.0)},
                {TEXT("Approach"), FVector2D(0.25, 0.72)},
                {TEXT("Putt"), FVector2D(0.05, 0.45)},
            };
            bool bExactLibrary = CandidateLibrary
                && CandidateLibrary->Entries.Num() == ExpectedMontages.Num();
            TSet<FName> ObservedFamilies;
            if (bExactLibrary)
            {
                for (const FDGThrowAnimationEntry& Entry
                    : CandidateLibrary->Entries)
                {
                    const FString* ExpectedPath =
                        ExpectedMontages.Find(Entry.MotionFamilyId);
                    const FName* ExpectedStyle =
                        ExpectedStyles.Find(Entry.MotionFamilyId);
                    const FVector2D* ExpectedPowerRange =
                        ExpectedPowerRanges.Find(Entry.MotionFamilyId);
                    const FString ActualPath =
                        Entry.Montage.ToSoftObjectPath().ToString();
                    if (!ExpectedPath
                        || !ExpectedStyle
                        || !ExpectedPowerRange
                        || Entry.ThrowType != EDGThrowType::Backhand
                        || Entry.Handedness != EDGHandedness::Right
                        || Entry.StyleId != *ExpectedStyle
                        || ActualPath != *ExpectedPath
                        || !FMath::IsNearlyEqual(
                            Entry.RecommendedPowerMin,
                            static_cast<float>(ExpectedPowerRange->X))
                        || !FMath::IsNearlyEqual(
                            Entry.RecommendedPowerMax,
                            static_cast<float>(ExpectedPowerRange->Y))
                        || ObservedFamilies.Contains(Entry.MotionFamilyId))
                    {
                        bExactLibrary = false;
                        break;
                    }
                    ObservedFamilies.Add(Entry.MotionFamilyId);
                }
                bExactLibrary = bExactLibrary
                    && ObservedFamilies.Num() == ExpectedMontages.Num();
            }
            if (CandidateDrive && CandidateApproach && CandidatePutt
                && CandidateDrive->GetSkeleton()
                && CandidateApproach->GetSkeleton() == CandidateDrive->GetSkeleton()
                && CandidatePutt->GetSkeleton() == CandidateDrive->GetSkeleton()
                && bExactLibrary)
            {
                ProductionDriveMontage = CandidateDrive;
                ProductionApproachMontage = CandidateApproach;
                ProductionPuttMontage = CandidatePutt;
                ProductionAnimationLibrary = CandidateLibrary;
                RHBHThrowMontage = CandidateDrive;
                ProductionMotionCandidateRevision =
                    DiscGolfProductionMotion::V7::AssetRevision;
                UE_LOG(LogDiscGolfTour, Display,
                    TEXT("PRODUCTION MOTION CANDIDATE OVERRIDE: v007 isolated visual evidence only; ActiveVersion=%s remains unchanged."),
                    DiscGolfProductionMotion::ActiveVersion);
            }
            else
            {
                UE_LOG(LogDiscGolfTour, Error,
                    TEXT("PRODUCTION MOTION CANDIDATE OVERRIDE FAILED CLOSED: exact v007 library/montage set is unavailable or inconsistent; accepted v006 remains selected."));
            }
        }
    }
#endif

    FDGFullCharacterCustomization StartupCustomization =
        DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    if (const UDiscGolfTourGameInstance* Instance =
            Cast<UDiscGolfTourGameInstance>(GetGameInstance()))
    {
        StartupCustomization = Instance->GetFullCharacterCustomization();
    }

#if DG_WITH_DEVELOPMENT_CONTENT
    // Session 5 may override the separately authored production candidate only
    // in an explicit unattended development process. Shipping never references
    // the prototype or synthetic pipeline montage.
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
#endif

    if (CharacterProfileTemplate && FrameworkThrowComponent)
    {
        RuntimeCharacterProfile = DuplicateObject<UDiscGolfCharacterProfile>(
            CharacterProfileTemplate, this, TEXT("RuntimeCharacterProfile"));
        if (RuntimeCharacterProfile)
        {
            RuntimeCharacterProfile->SetFlags(RF_Transient);
            FrameworkThrowComponent->CharacterProfile = RuntimeCharacterProfile;

            FDGBodyProfile SavedBody = StartupCustomization.Body;
            FDGThrowStyle SavedStyle = StartupCustomization.ThrowStyle;
            EDGHandedness SavedHandedness =
                StartupCustomization.Identity.Handedness;
#if DG_WITH_DEVELOPMENT_CONTENT
            FString Session4OverrideLabel;
            if (ResolveSession4ProfileOverride(
                    this, SavedBody, SavedStyle, SavedHandedness, Session4OverrideLabel))
            {
                UE_LOG(LogDiscGolfTour, Display,
                    TEXT("SESSION 4 PROFILE OVERRIDE: %s (transient, save slot unchanged)."),
                    *Session4OverrideLabel);
            }
#endif
            StartupCustomization.Body = SavedBody;
            StartupCustomization.ThrowStyle = SavedStyle;
            StartupCustomization.Identity.Handedness = SavedHandedness;
            ApplyCharacterProfileUnchecked(SavedBody, SavedStyle, SavedHandedness);
        }
    }

    const bool bHasSkeletalAsset = SkeletalMesh && SkeletalMesh->GetSkeletalMeshAsset() != nullptr;
    if (SkeletalMesh) SkeletalMesh->SetVisibility(bHasSkeletalAsset);
    const bool bHasModularHead = bHasSkeletalAsset && ModularHeadMesh
        && ModularHeadMesh->GetSkeletalMeshAsset() != nullptr;
    if (ModularHeadMesh)
    {
        ModularHeadMesh->SetLeaderPoseComponent(SkeletalMesh);
        ModularHeadMesh->SetVisibility(bHasModularHead);
    }
    if (BodyMesh) BodyMesh->SetVisibility(!bHasSkeletalAsset);
    if (HeadMesh) HeadMesh->SetVisibility(!bHasSkeletalAsset);
    if (PresentationComponent) PresentationComponent->SetSkeletalAssetsReady(bHasSkeletalAsset);
    if (OutfitComponent)
    {
        OutfitComponent->Catalog = LoadObject<UDiscGolfOutfitCatalog>(
            nullptr, DiscGolfOutfitRuntime::CatalogObjectPath);
        OutfitComponent->OnBodyCoverageChanged.AddUniqueDynamic(
            this, &ADiscGolferPawn::HandleOutfitCoverageChanged);

        const FDGOutfitLoadout SavedLoadout = StartupCustomization.Outfit;
        FString OutfitStatus;
        if (!ApplyOutfitLoadoutTransactionally(SavedLoadout, true, OutfitStatus)
            || !OutfitStatus.IsEmpty())
        {
            UE_LOG(LogDiscGolfTour, Warning, TEXT("Outfit startup recovery: %s"), *OutfitStatus);
        }
    }
    if (CharacterCustomization)
    {
        CharacterCustomization->CosmeticCatalog = LoadObject<UDiscGolfCosmeticCatalog>(
            nullptr, DiscGolfFullCharacterRuntime::CosmeticCatalogObjectPath);
        const FDiscGolfFullCustomizationResolution Resolution =
            DiscGolfFullCharacterRuntime::ResolveForRuntime(
                StartupCustomization,
                CharacterCustomization->CosmeticCatalog,
                GetOutfitCatalog());
        CharacterCustomization->Current = Resolution.Character;
        CharacterCustomization->Current.Outfit = GetCurrentOutfitLoadout();
        ApplyFullCustomizationVisuals();
        if (!Resolution.Warnings.IsEmpty())
        {
            UE_LOG(LogDiscGolfTour, Warning,
                TEXT("Full-character startup recovery: %s"),
                *FString::Join(Resolution.Warnings, TEXT(" ")));
        }
    }
    if (RHBHThrowAdapter)
    {
        RHBHThrowAdapter->Configure(FrameworkThrowComponent, SkeletalMesh, HeldDiscVisual);
        RHBHThrowAdapter->ConfigureIngressContracts(ThrowController, DiscBag);
        RHBHThrowAdapter->GetAuthoritativeLaunchDelegate().BindUObject(
            this, &ADiscGolferPawn::HandleAnimatedRHBHRelease);
        RHBHThrowAdapter->OnThrowRecovered.AddUniqueDynamic(
            this, &ADiscGolferPawn::HandleAnimatedThrowRecovered);
    }
    const FDGFullCharacterCustomization& BackendCustomization =
        CharacterCustomization
        ? CharacterCustomization->Current
        : StartupCustomization;
    FString BackendStatus;
    const bool bBackendApplied = ApplyAvatarBackendForCustomization(
        BackendCustomization, true, BackendStatus);
    const bool bMetaHumanFallback =
        BackendCustomization.AvatarBackendId
            == FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId)
        && (!AvatarBackendComponent
            || !AvatarBackendComponent->IsVisualBackendReady());
    if (!bBackendApplied || bMetaHumanFallback)
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Avatar backend startup recovery: %s"), *BackendStatus);
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
    EDGHandedness Handedness,
    bool bAllowAvatarBackendFallback)
{
    if (!RuntimeCharacterProfile || !IsCharacterProfileChangeSafe())
    {
        return false;
    }

    if (CharacterCustomization)
    {
        FDGFullCharacterCustomization Candidate =
            GetCurrentFullCharacterCustomization();
        Candidate.Body = Body;
        Candidate.ThrowStyle = Style;
        Candidate.Identity.Handedness = Handedness;
        FString IgnoredStatus;
        return ApplyFullCharacterCustomizationTransactionally(
            Candidate, true, IgnoredStatus, bAllowAvatarBackendFallback);
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
    if (CharacterCustomization)
    {
        CharacterCustomization->Current.Body = RuntimeCharacterProfile->Body;
        CharacterCustomization->Current.ThrowStyle = RuntimeCharacterProfile->ThrowStyle;
        CharacterCustomization->Current.Identity.Handedness =
            RuntimeCharacterProfile->Handedness;
    }
    RefreshOutfitForCurrentBodyProfile();
    RefreshCharacterProfilePresentation();
}

FDGFullCharacterCustomization ADiscGolferPawn::GetCurrentFullCharacterCustomization() const
{
    FDGFullCharacterCustomization Result = CharacterCustomization
        ? CharacterCustomization->Current
        : DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    if (RuntimeCharacterProfile)
    {
        Result.Body = RuntimeCharacterProfile->Body;
        Result.ThrowStyle = RuntimeCharacterProfile->ThrowStyle;
        Result.Identity.Handedness = RuntimeCharacterProfile->Handedness;
        Result.Identity.DisplayName = RuntimeCharacterProfile->DisplayName.ToString();
    }
    Result.Outfit = GetCurrentOutfitLoadout();
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Result);
    return Result;
}

UDiscGolfCosmeticCatalog* ADiscGolferPawn::GetCosmeticCatalog() const
{
    return CharacterCustomization ? CharacterCustomization->CosmeticCatalog.Get() : nullptr;
}

bool ADiscGolferPawn::PreviewFullCharacterCustomization(
    const FDGFullCharacterCustomization& Requested,
    FString& OutStatus)
{
    if (!IsCharacterProfileChangeSafe())
    {
        OutStatus = TEXT("Full-character preview is blocked until gameplay returns to a safe state.");
        return false;
    }
    return ApplyFullCharacterCustomizationTransactionally(
        Requested, true, OutStatus);
}

bool ADiscGolferPawn::ApplyFullCharacterCustomizationTransactionally(
    const FDGFullCharacterCustomization& Requested,
    bool bAllowUnavailableItems,
    FString& OutStatus,
    bool bAllowAvatarBackendFallback)
{
    OutStatus.Reset();
    if (!CharacterCustomization || !RuntimeCharacterProfile
        || !SkeletalMesh || !OutfitComponent)
    {
        OutStatus = TEXT("Full-character runtime is unavailable; the current character was retained.");
        return false;
    }

    const FDGFullCharacterCustomization Previous =
        GetCurrentFullCharacterCustomization();
    const FDiscGolfFullCustomizationResolution Resolution =
        DiscGolfFullCharacterRuntime::ResolveForRuntime(
            Requested, GetCosmeticCatalog(), GetOutfitCatalog());
    if (!Resolution.bAllCosmeticsResolved && !bAllowUnavailableItems)
    {
        OutStatus = FString::Join(Resolution.Warnings, TEXT(" "));
        return false;
    }

    CharacterCustomization->Current = Resolution.Character;
    ApplyCharacterProfileUnchecked(
        Resolution.Character.Body,
        Resolution.Character.ThrowStyle,
        Resolution.Character.Identity.Handedness);
    RuntimeCharacterProfile->DisplayName =
        FText::FromString(Resolution.Character.Identity.DisplayName);

    FString OutfitStatus;
    if (!ApplyOutfitLoadoutTransactionally(
            Resolution.Character.Outfit, true, OutfitStatus))
    {
        CharacterCustomization->Current = Previous;
        ApplyCharacterProfileUnchecked(
            Previous.Body, Previous.ThrowStyle, Previous.Identity.Handedness);
        RuntimeCharacterProfile->DisplayName =
            FText::FromString(Previous.Identity.DisplayName);
        FString RollbackStatus;
        ApplyOutfitLoadoutTransactionally(
            Previous.Outfit, true, RollbackStatus);
        ApplyFullCustomizationVisuals();
        OutStatus = OutfitStatus.IsEmpty()
            ? TEXT("Full-character preview failed; the previous character was restored.")
            : OutfitStatus;
        return false;
    }

    CharacterCustomization->Current.Outfit = GetCurrentOutfitLoadout();
    ApplyFullCustomizationVisuals();
    FString BackendStatus;
    if (!ApplyAvatarBackendForCustomization(
            CharacterCustomization->Current,
            bAllowAvatarBackendFallback,
            BackendStatus))
    {
        CharacterCustomization->Current = Previous;
        ApplyCharacterProfileUnchecked(
            Previous.Body, Previous.ThrowStyle, Previous.Identity.Handedness);
        RuntimeCharacterProfile->DisplayName =
            FText::FromString(Previous.Identity.DisplayName);
        FString RollbackStatus;
        ApplyOutfitLoadoutTransactionally(
            Previous.Outfit, true, RollbackStatus);
        ApplyFullCustomizationVisuals();
        FString BackendRollbackStatus;
        ApplyAvatarBackendForCustomization(
            Previous, true, BackendRollbackStatus);
        OutStatus = BackendStatus.IsEmpty()
            ? TEXT("Visual backend preview failed; the previous character was restored.")
            : BackendStatus;
        return false;
    }
    TArray<FString> StatusParts = Resolution.Warnings;
    if (!OutfitStatus.IsEmpty())
    {
        StatusParts.Add(OutfitStatus);
    }
    if (!BackendStatus.IsEmpty())
    {
        StatusParts.Add(BackendStatus);
    }
    OutStatus = FString::Join(StatusParts, TEXT(" "));
    return true;
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

    if (CharacterCustomization)
    {
        CharacterCustomization->Current.Outfit = OutfitComponent->CurrentLoadout;
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

    const bool bShouldHideHair =
        CoveredOutfitBodyRegions.Contains(EDGBodyRegion::Hair);
    if (bHairHiddenByOutfitCoverage != bShouldHideHair)
    {
        bHairHiddenByOutfitCoverage = bShouldHideHair;
        RebuildCustomizationHairForCoverage();
    }
}

void ADiscGolferPawn::ApplyFullCustomizationVisuals()
{
    if (!CharacterCustomization || !SkeletalMesh)
    {
        return;
    }
    if (RuntimeCharacterProfile)
    {
        RuntimeCharacterProfile->DisplayName =
            FText::FromString(CharacterCustomization->Current.Identity.DisplayName);
    }
    CharacterCustomization->ApplyBodyMorphs(SkeletalMesh);
    if (ModularHeadMesh && ModularHeadMesh->GetSkeletalMeshAsset())
    {
        CharacterCustomization->ApplyBodyMorphs(ModularHeadMesh);
        CharacterCustomization->ApplyFaceMorphs(ModularHeadMesh);
        CharacterCustomization->ApplySkinAndEyeMaterials(
            nullptr, ModularHeadMesh);
        const float ScarProxy =
            CharacterCustomization->Current.Appearance.ScarId
                == FName(TEXT("scar_none")) ? 0.0f : 1.0f;
        const float TattooProxy =
            CharacterCustomization->Current.Appearance.TattooIds.IsEmpty()
                ? 0.0f : 1.0f;
        for (int32 MaterialIndex = 0;
            MaterialIndex < ModularHeadMesh->GetNumMaterials();
            ++MaterialIndex)
        {
            UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(
                ModularHeadMesh->GetMaterial(MaterialIndex));
            if (MID)
            {
                MID->SetScalarParameterValue(TEXT("DG_ScarProxy"), ScarProxy);
                MID->SetScalarParameterValue(TEXT("DG_TattooProxy"), TattooProxy);
            }
        }
        RebuildCustomizationHairForCoverage();
        ModularHeadMesh->RefreshBoneTransforms();
        ModularHeadMesh->MarkRenderDynamicDataDirty();
    }
    SkeletalMesh->MarkRenderDynamicDataDirty();
    SetDGProxyPresentationVisible(bDGProxyPresentationVisible);
}

void ADiscGolferPawn::RebuildCustomizationHairForCoverage()
{
    if (!CharacterCustomization || !ModularHeadMesh
        || !ModularHeadMesh->GetSkeletalMeshAsset())
    {
        return;
    }

    // Outfit coverage suppresses only the visible hairstyle. The stable
    // selection remains in Current and therefore survives Apply, Cancel,
    // save/reload and removing the hat; beard and eyebrows always rebuild.
    const FName SelectedHairStyleId =
        CharacterCustomization->Current.Hair.HairStyleId;
    if (bHairHiddenByOutfitCoverage)
    {
        CharacterCustomization->Current.Hair.HairStyleId = TEXT("hair_none");
    }
    CharacterCustomization->RebuildHair(ModularHeadMesh);
    CharacterCustomization->Current.Hair.HairStyleId = SelectedHairStyleId;
    SetDGProxyPresentationVisible(bDGProxyPresentationVisible);
}

bool ADiscGolferPawn::IsDGProxyPresentationVisible() const
{
    TInlineComponentArray<UPrimitiveComponent*> PawnPrimitives;
    GetComponents(PawnPrimitives);
    for (const UPrimitiveComponent* Primitive : PawnPrimitives)
    {
        if (!IsValid(Primitive) || Primitive == HeldDiscVisual
            || Primitive == Capsule)
        {
            continue;
        }
        const bool bProxyPrimitive = Primitive == SkeletalMesh
            || Primitive == ModularHeadMesh
            || Primitive == BodyMesh
            || Primitive == HeadMesh
            || (SkeletalMesh && Primitive->IsAttachedTo(SkeletalMesh))
            || (ModularHeadMesh
                && Primitive->IsAttachedTo(ModularHeadMesh));
        if (bProxyPrimitive && Primitive->IsVisible()
            && !Primitive->bHiddenInGame)
        {
            return true;
        }
    }
    return false;
}

void ADiscGolferPawn::SetDGProxyPresentationVisible(bool bVisible)
{
    bDGProxyPresentationVisible = bVisible;
    const bool bHasSkeletalAsset = SkeletalMesh
        && SkeletalMesh->GetSkeletalMeshAsset();
    const bool bHasModularHead = bHasSkeletalAsset && ModularHeadMesh
        && ModularHeadMesh->GetSkeletalMeshAsset();

    TInlineComponentArray<UPrimitiveComponent*> PawnPrimitives;
    GetComponents(PawnPrimitives);
    for (UPrimitiveComponent* Primitive : PawnPrimitives)
    {
        if (!IsValid(Primitive) || Primitive == HeldDiscVisual
            || Primitive == Capsule)
        {
            continue;
        }
        const bool bProxyPrimitive = Primitive == SkeletalMesh
            || Primitive == ModularHeadMesh
            || Primitive == BodyMesh
            || Primitive == HeadMesh
            || (SkeletalMesh && Primitive->IsAttachedTo(SkeletalMesh))
            || (ModularHeadMesh && Primitive->IsAttachedTo(ModularHeadMesh));
        if (!bProxyPrimitive)
        {
            continue;
        }

        bool bPrimitiveVisible = bVisible;
        if (Primitive == SkeletalMesh)
        {
            bPrimitiveVisible = bVisible && bHasSkeletalAsset;
        }
        else if (Primitive == ModularHeadMesh)
        {
            bPrimitiveVisible = bVisible && bHasModularHead;
        }
        else if (Primitive == BodyMesh || Primitive == HeadMesh)
        {
            bPrimitiveVisible = bVisible && !bHasSkeletalAsset;
        }
        Primitive->SetVisibility(bPrimitiveVisible, false);
        Primitive->SetHiddenInGame(!bPrimitiveVisible, false);
    }
}

bool ADiscGolferPawn::ApplyAvatarBackendForCustomization(
    const FDGFullCharacterCustomization& Customization,
    bool bAllowDGMasterFallback,
    FString& OutStatus)
{
    OutStatus.Reset();
    if (!AvatarBackendComponent || !SkeletalMesh)
    {
        OutStatus = TEXT("Avatar backend component or DG animation source is unavailable.");
        SetDGProxyPresentationVisible(true);
        return bAllowDGMasterFallback;
    }
    const auto RetainVerifiedVisualOrShowProxy = [this]()
    {
        const bool bRetainedVerifiedVisual =
            AvatarBackendComponent->IsPresentationPolicyVerified();
        if (!bRetainedVerifiedVisual
            && AvatarBackendComponent->IsVisualBackendReady())
        {
            AvatarBackendComponent->GetPresentationPolicyStatus();
            AvatarBackendComponent->DestroyVisualBackend();
        }
        SetDGProxyPresentationVisible(!bRetainedVerifiedVisual);
    };

    UDiscGolfAvatarBackendProfile* MetaHumanProfile = nullptr;
    if (Customization.AvatarBackendId
        == FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId))
    {
        MetaHumanProfile = LoadObject<UDiscGolfAvatarBackendProfile>(
            nullptr,
            DiscGolfAvatarBackendRuntime::MetaHumanDefaultProfileObjectPath);
    }
    const FDiscGolfAvatarBackendResolution BackendResolution =
        DiscGolfAvatarBackendRuntime::ResolveBackend(
            Customization.AvatarBackendId,
            MetaHumanProfile);

    if (!BackendResolution.bMetaHumanAttemptAllowed)
    {
        SetDGProxyPresentationVisible(true);
        // Finalize any already-promoted MetaHuman evidence before the
        // framework's nonvirtual destroy seam clears the active actor.
        AvatarBackendComponent->GetPresentationPolicyStatus();
        AvatarBackendComponent->DestroyVisualBackend();
        AvatarBackendComponent->BackendProfile = nullptr;
        if (Customization.AvatarBackendId
            == FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId))
        {
            UE_LOG(LogDiscGolfTour, Warning,
                TEXT("Realistic character backend resolution failed: %s"),
                *BackendResolution.Status);
            OutStatus = TEXT("Realistic character backend is unavailable; DGMaster fallback remains active.");
            return bAllowDGMasterFallback;
        }
        return true;
    }

    AvatarBackendComponent->BackendProfile = MetaHumanProfile;
    const EDGMetaHumanPresentationPolicy DesiredPresentationPolicy =
        bCharacterCreatorPreviewActive
        ? EDGMetaHumanPresentationPolicy::CharacterCreator
        : EDGMetaHumanPresentationPolicy::GameplayPerformance;
    if (!AvatarBackendComponent->SetPresentationPolicy(
            DesiredPresentationPolicy))
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Realistic character %s policy verification failed: %s"),
            DesiredPresentationPolicy
                    == EDGMetaHumanPresentationPolicy::CharacterCreator
                ? TEXT("CharacterCreator")
                : TEXT("GameplayPerformance"),
            *AvatarBackendComponent->GetPresentationPolicyStatus());
        OutStatus = FString::Printf(
            TEXT("Realistic character %s policy was not verified; the last verified visual or DGMaster fallback was retained."),
            DesiredPresentationPolicy
                    == EDGMetaHumanPresentationPolicy::CharacterCreator
                ? TEXT("CharacterCreator")
                : TEXT("GameplayPerformance"));
        if (bAllowDGMasterFallback)
        {
            AvatarBackendComponent->GetPresentationPolicyStatus();
            AvatarBackendComponent->DestroyVisualBackend();
            AvatarBackendComponent->BackendProfile = nullptr;
            SetDGProxyPresentationVisible(true);
            return true;
        }
        RetainVerifiedVisualOrShowProxy();
        return false;
    }
    const bool bSameVerifiedBackend =
        AvatarBackendComponent->IsVisualBackendReady()
        && AvatarBackendComponent->GetActiveBackendProfile()
            == MetaHumanProfile;
    const bool bReady = bSameVerifiedBackend
        ? AvatarBackendComponent->ApplyCustomizationToVisual(Customization)
        : AvatarBackendComponent->BuildVisualBackend(
            SkeletalMesh, Customization);
    if (!bReady || !AvatarBackendComponent->IsVisualBackendReady()
        || AvatarBackendComponent->GetVerifiedPresentationPolicy()
            != DesiredPresentationPolicy
        || !AvatarBackendComponent->IsPresentationPolicyVerified())
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Realistic character adapter verification failed: %s"),
            *AvatarBackendComponent->GetLastAdapterStatus());
        OutStatus = TEXT("Realistic character verification failed; DGMaster remains visible.");
        if (bAllowDGMasterFallback)
        {
            AvatarBackendComponent->GetPresentationPolicyStatus();
            AvatarBackendComponent->DestroyVisualBackend();
            AvatarBackendComponent->BackendProfile = nullptr;
            SetDGProxyPresentationVisible(true);
            return true;
        }
        RetainVerifiedVisualOrShowProxy();
        return false;
    }

    SetDGProxyPresentationVisible(false);
    OutStatus = TEXT("Verified realistic character presentation is active; DGMaster remains the hidden animation and gameplay authority.");
    return true;
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

bool ADiscGolferPawn::BeginCharacterCreatorPreview()
{
    if (!CameraBoom || !Camera || !SkeletalMesh || !AvatarBackendComponent)
    {
        return false;
    }
    if (!AvatarBackendComponent->SetPresentationPolicy(
            EDGMetaHumanPresentationPolicy::CharacterCreator))
    {
        if (!AvatarBackendComponent->IsPresentationPolicyVerified())
        {
            if (AvatarBackendComponent->IsVisualBackendReady())
            {
                AvatarBackendComponent->GetPresentationPolicyStatus();
                AvatarBackendComponent->DestroyVisualBackend();
            }
            SetDGProxyPresentationVisible(true);
        }
        UE_LOG(
            LogDiscGolfTour,
            Error,
            TEXT("Character creator preview rejected before camera/tick mutation: %s"),
            *AvatarBackendComponent->GetPresentationPolicyStatus());
        return false;
    }
    if (bCharacterCreatorPreviewActive)
    {
        return true;
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
    return true;
}

bool ADiscGolferPawn::EndCharacterCreatorPreview(bool bRestoreView)
{
    if (!AvatarBackendComponent
        || !AvatarBackendComponent->SetPresentationPolicy(
            EDGMetaHumanPresentationPolicy::GameplayPerformance))
    {
        if (AvatarBackendComponent
            && !AvatarBackendComponent->IsPresentationPolicyVerified())
        {
            if (AvatarBackendComponent->IsVisualBackendReady())
            {
                AvatarBackendComponent->GetPresentationPolicyStatus();
                AvatarBackendComponent->DestroyVisualBackend();
            }
            SetDGProxyPresentationVisible(true);
        }
        UE_LOG(
            LogDiscGolfTour,
            Error,
            TEXT("Character creator preview remains active because gameplay policy was not verified: %s"),
            AvatarBackendComponent
                ? *AvatarBackendComponent->GetPresentationPolicyStatus()
                : TEXT("avatar backend component is unavailable"));
        return false;
    }
    if (!bCharacterCreatorPreviewActive)
    {
        return true;
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
    return true;
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

void ADiscGolferPawn::ZoomCharacterCreatorPreview(float DeltaArmLength)
{
    if (!bCharacterCreatorPreviewActive || !CameraBoom
        || !FMath::IsFinite(DeltaArmLength))
    {
        return;
    }
    CameraBoom->TargetArmLength = FMath::Clamp(
        CameraBoom->TargetArmLength + DeltaArmLength, 300.0f, 620.0f);
    if (CameraBoom->IsRegistered())
    {
        CameraBoom->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
    }
    if (const APlayerController* PlayerController =
            Cast<APlayerController>(GetController());
        PlayerController && PlayerController->PlayerCameraManager)
    {
        PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
    }
}

bool ADiscGolferPawn::SetCharacterCreatorPreviewFraming(
    float TargetArmLength,
    float BoomPitchDegrees,
    float SubjectYawOffsetDegrees,
    float VerticalSocketOffsetCm)
{
    if (!bCharacterCreatorPreviewActive || !CameraBoom || !SkeletalMesh
        || !FMath::IsFinite(TargetArmLength)
        || !FMath::IsFinite(BoomPitchDegrees)
        || !FMath::IsFinite(SubjectYawOffsetDegrees)
        || !FMath::IsFinite(VerticalSocketOffsetCm))
    {
        return false;
    }

    CameraBoom->TargetArmLength = FMath::Clamp(
        TargetArmLength, 300.0f, 680.0f);
    FVector SocketOffset = CameraBoom->SocketOffset;
    SocketOffset.Z = FMath::Clamp(VerticalSocketOffsetCm, 0.0f, 120.0f);
    CameraBoom->SocketOffset = SocketOffset;
    // BeginCharacterCreatorPreview establishes the intentional front-facing
    // 180-degree creator yaw. Preserve that active creator orbit while making
    // arm length and pitch absolute, rather than restoring gameplay yaw.
    FRotator BoomRotation = CameraBoom->GetRelativeRotation();
    BoomRotation.Pitch = FMath::Clamp(BoomPitchDegrees, -25.0f, 10.0f);
    CameraBoom->SetRelativeRotation(BoomRotation);

    FRotator SubjectRotation = SavedPreviewSkeletalRotation;
    SubjectRotation.Yaw = FMath::UnwindDegrees(
        SubjectRotation.Yaw
        + FMath::Clamp(SubjectYawOffsetDegrees, -45.0f, 45.0f));
    SkeletalMesh->SetRelativeRotation(SubjectRotation);

    if (!CameraBoom->IsRegistered())
    {
        return false;
    }
    CameraBoom->TickComponent(
        0.0f, ELevelTick::LEVELTICK_All, nullptr);
    if (const APlayerController* PlayerController =
            Cast<APlayerController>(GetController());
        PlayerController && PlayerController->PlayerCameraManager)
    {
        PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
    }
    return FMath::IsNearlyEqual(
            CameraBoom->TargetArmLength,
            FMath::Clamp(TargetArmLength, 300.0f, 680.0f))
        && FMath::IsNearlyEqual(
            CameraBoom->GetRelativeRotation().Pitch,
            FMath::Clamp(BoomPitchDegrees, -25.0f, 10.0f),
            0.01f)
        && FMath::IsNearlyEqual(
            CameraBoom->SocketOffset.Z,
            FMath::Clamp(VerticalSocketOffsetCm, 0.0f, 120.0f),
            0.01f);
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
#if DG_WITH_DEVELOPMENT_CONTENT
    EnhancedInput->BindAction(InputConfig->CycleRegressionPresetAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputCycleRegressionPreset);
    EnhancedInput->BindAction(InputConfig->RunRegressionPresetAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputRunRegressionPreset);
    EnhancedInput->BindAction(InputConfig->RunRegressionSuiteAction, ETriggerEvent::Started, this, &ADiscGolferPawn::InputRunRegressionSuite);
#endif
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
    const auto IsFiniteVector = [](const FVector& Value)
    {
        return FMath::IsFinite(Value.X)
            && FMath::IsFinite(Value.Y)
            && FMath::IsFinite(Value.Z);
    };

    const FVector ActorLocation = GetActorLocation();
    if (!IsFiniteVector(WorldLocation) || !IsFiniteVector(ActorLocation))
    {
        return;
    }

    const FVector ToTarget = WorldLocation - ActorLocation;
    const float HorizontalDistanceSquared = FVector2D(ToTarget.X, ToTarget.Y).SizeSquared();
    if (!IsFiniteVector(ToTarget)
        || !FMath::IsFinite(HorizontalDistanceSquared)
        || FMath::IsNearlyZero(HorizontalDistanceSquared))
    {
        return;
    }

    const FRotator LookRotation = ToTarget.Rotation();
    const FRotator NewRotation(0.0f, LookRotation.Yaw, 0.0f);
    if (FMath::IsFinite(LookRotation.Yaw) && !NewRotation.ContainsNaN())
    {
        SetActorRotation(NewRotation);
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
    EndPreCommitThrowPresentation();
}

bool ADiscGolferPawn::IsAnimatedThrowActive() const
{
    return ActiveRHBHMontageAttemptSerial > 0
        || (RHBHThrowAdapter && RHBHThrowAdapter->IsThrowActive());
}

FString ADiscGolferPawn::GetActiveRHBHThrowMontagePath() const
{
    return RHBHThrowMontage ? RHBHThrowMontage->GetPathName() : FString();
}

void ADiscGolferPawn::RefreshPreCommitThrowPresentation()
{
    if (!FrameworkThrowComponent
        || !ThrowController
        || (RHBHThrowAdapter && RHBHThrowAdapter->IsThrowActive())
        || (FrameworkThrowComponent->bThrowActive
            && FrameworkThrowComponent->IsThrowCommitted()))
    {
        return;
    }

    FDGThrowIntent Intent;
    Intent.ThrowType = ThrowController->GetThrowStyle() == EThrowStyle::Forehand
        ? EDGThrowType::Forehand
        : EDGThrowType::Backhand;
    Intent.Power01 = ThrowController->GetPower01();
    Intent.HyzerDegrees = ThrowController->GetHyzerDeg();
    Intent.NoseDegrees = ThrowController->GetNoseDeg();
    // The rig expects a local cosmetic delta, not the pawn's absolute world
    // heading. Direction remains locked in the gameplay controller.
    Intent.AimYawDegrees = 0.0f;
    FrameworkThrowComponent->SetThrowIntent(Intent);
    if (!FrameworkThrowComponent->bThrowActive)
    {
        FrameworkThrowComponent->BeginAimPreview();
    }
}

void ADiscGolferPawn::EndPreCommitThrowPresentation()
{
    if (FrameworkThrowComponent
        && FrameworkThrowComponent->bThrowActive
        && !FrameworkThrowComponent->IsThrowCommitted())
    {
        FrameworkThrowComponent->CancelThrow();
    }
}

UAnimMontage* ADiscGolferPawn::ResolveRHBHThrowMontage(
    const FThrowCommand& AuthoritativeCommand,
    EGolferAnimationFamily AnimationFamily) const
{
    const auto IsUsableMontage = [this](const UAnimMontage* Montage)
    {
        if (!IsMontageLifecycleSafe(Montage))
        {
            return false;
        }

        const USkeletalMesh* MeshAsset = SkeletalMesh
            ? SkeletalMesh->GetSkeletalMeshAsset() : nullptr;
        return !MeshAsset
            || !MeshAsset->GetSkeleton()
            || Montage->GetSkeleton() == MeshAsset->GetSkeleton();
    };

    UAnimMontage* BestLibraryMontage = nullptr;
    float BestSelectionScore = TNumericLimits<float>::Max();
    const FName ExpectedFamilyId = MotionFamilyId(AnimationFamily);
    if (ProductionAnimationLibrary)
    {
        for (const FDGThrowAnimationEntry& Entry : ProductionAnimationLibrary->Entries)
        {
            if (Entry.ThrowType != EDGThrowType::Backhand
                || Entry.Handedness != EDGHandedness::Right
                || Entry.MotionFamilyId != ExpectedFamilyId
                || !IsRecommendedPowerRangeValid(Entry)
                || AuthoritativeCommand.Power01 < Entry.RecommendedPowerMin
                || AuthoritativeCommand.Power01 > Entry.RecommendedPowerMax
                || Entry.Montage.IsNull())
            {
                continue;
            }

            UAnimMontage* Candidate = Entry.Montage.LoadSynchronous();
            if (!IsUsableMontage(Candidate))
            {
                continue;
            }

            const float RangeWidth = FMath::Max(
                Entry.RecommendedPowerMax - Entry.RecommendedPowerMin,
                UE_KINDA_SMALL_NUMBER);
            const float RangeMidpoint =
                (Entry.RecommendedPowerMin + Entry.RecommendedPowerMax) * 0.5f;
            const float SelectionScore =
                FMath::Abs(AuthoritativeCommand.Power01 - RangeMidpoint) / RangeWidth;
            if (SelectionScore < BestSelectionScore)
            {
                BestSelectionScore = SelectionScore;
                BestLibraryMontage = Candidate;
            }
        }
    }

    if (BestLibraryMontage)
    {
        return BestLibraryMontage;
    }

    UAnimMontage* FallbackMontage = nullptr;
    switch (AnimationFamily)
    {
        case EGolferAnimationFamily::Approach:
            FallbackMontage = ProductionApproachMontage;
            break;
        case EGolferAnimationFamily::Putt:
            FallbackMontage = ProductionPuttMontage;
            break;
        default:
            FallbackMontage = ProductionDriveMontage;
            break;
    }
    return IsUsableMontage(FallbackMontage) ? FallbackMontage : nullptr;
}

void ADiscGolferPawn::StopAndClearActiveRHBHMontage(float BlendOutSeconds)
{
    UAnimMontage* MontageToStop = ActiveRHBHThrowMontage;
    ActiveRHBHThrowMontage = nullptr;
    ActiveRHBHMontageAttemptSerial = 0;
    if (!SkeletalMesh || !MontageToStop)
    {
        return;
    }

    if (UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
        AnimInstance && AnimInstance->Montage_IsPlaying(MontageToStop))
    {
        AnimInstance->Montage_Stop(FMath::Max(0.0f, BlendOutSeconds), MontageToStop);
    }
}

bool ADiscGolferPawn::TryStartAnimatedRHBHThrow(const FThrowCommand& AuthoritativeCommand)
{
    if (!DiscGolfMath::IsThrowCommandValid(AuthoritativeCommand)
        || AuthoritativeCommand.ThrowStyle != EThrowStyle::Backhand
        || AuthoritativeCommand.Handedness != EDGHandedness::Right
        || !RuntimeCharacterProfile
        || RuntimeCharacterProfile->Handedness != EDGHandedness::Right
        || !ThrowController
        || ThrowController->GetShotContext() != AuthoritativeCommand.ShotContext
        || !PresentationComponent)
    {
        return false;
    }

    const EGolferAnimationFamily AnimationFamily =
        PresentationComponent->GetAnimationFamily();
    if (!DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            AuthoritativeCommand.ShotContext, AnimationFamily))
    {
        return false;
    }

    EndPreCommitThrowPresentation();

    if (!bSession5PipelineValidationMontageActive)
    {
        RHBHThrowMontage = ResolveRHBHThrowMontage(
            AuthoritativeCommand, AnimationFamily);
    }
    if (!RHBHThrowAdapter || !RHBHThrowMontage || !SkeletalMesh)
    {
        return false;
    }

    if (bSession5PipelineValidationMontageActive
        && !IsMontageLifecycleSafe(RHBHThrowMontage))
    {
        return false;
    }

    UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
    if (!AnimInstance || !RHBHThrowAdapter->TryBeginRHBHThrow(AuthoritativeCommand))
    {
        return false;
    }

    const int64 AttemptSerial = RHBHThrowAdapter->GetAttemptSerial();
    ActiveRHBHThrowMontage = RHBHThrowMontage;
    ActiveRHBHMontageAttemptSerial = AttemptSerial;
    const float PlayedDuration = AnimInstance->Montage_Play(
        RHBHThrowMontage, ComputeThrowMontagePlayRate(AuthoritativeCommand));
    if (PlayedDuration <= 0.0f)
    {
        RHBHThrowAdapter->CancelBeforeRelease();
        StopAndClearActiveRHBHMontage(0.0f);
        return false;
    }

    const FAnimMontageInstance* MontageInstance =
        AnimInstance->GetActiveInstanceForMontage(RHBHThrowMontage);
    if (!MontageInstance
        || !FrameworkThrowComponent
        || !FrameworkThrowComponent->BindCommittedMontageInstance(
            AttemptSerial, MontageInstance->GetInstanceID(), RHBHThrowMontage))
    {
        RHBHThrowAdapter->CancelBeforeRelease();
        StopAndClearActiveRHBHMontage(0.0f);
        return false;
    }

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(
        this,
        &ADiscGolferPawn::HandleRHBHMontageEnded,
        AttemptSerial);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, RHBHThrowMontage);
    return true;
}

bool ADiscGolferPawn::CancelAnimatedThrowBeforeRelease()
{
    return RHBHThrowAdapter && RHBHThrowAdapter->CancelBeforeRelease();
}

void ADiscGolferPawn::CancelAnimatedThrow()
{
    if (RHBHThrowAdapter && RHBHThrowAdapter->IsThrowActive())
    {
        if (RHBHThrowAdapter->IsAwaitingRelease())
        {
            RHBHThrowAdapter->CancelBeforeRelease();
        }
        else
        {
            RHBHThrowAdapter->RecoverInterruptedThrow();
        }
        return;
    }

    // ThrowFinished releases gameplay/equipment ownership before the last
    // recovery frames blend out. Explicit UI/reset cancellation may still
    // retire that presentation tail without touching the released disc.
    if (ActiveRHBHMontageAttemptSerial > 0)
    {
        if (FrameworkThrowComponent)
        {
            FrameworkThrowComponent->CancelThrow();
        }
        StopAndClearActiveRHBHMontage(0.05f);
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

void ADiscGolferPawn::HandleRHBHMontageEnded(
    UAnimMontage* Montage,
    bool bInterrupted,
    int64 ExpectedAttemptSerial)
{
    (void)bInterrupted;
    if (ExpectedAttemptSerial <= 0
        || ExpectedAttemptSerial != ActiveRHBHMontageAttemptSerial
        || Montage != ActiveRHBHThrowMontage
        || !RHBHThrowAdapter
        || RHBHThrowAdapter->GetAttemptSerial() != ExpectedAttemptSerial)
    {
        return;
    }

    if (RHBHThrowAdapter->IsThrowActive())
    {
        // An interruption or a montage that reached its end without the
        // required ThrowFinished notify must close the token-matched attempt.
        if (!RHBHThrowAdapter->RecoverInterruptedThrow())
        {
            if (FrameworkThrowComponent)
            {
                FrameworkThrowComponent->CancelThrow();
            }
            StopAndClearActiveRHBHMontage(0.0f);
        }
        return;
    }

    ActiveRHBHThrowMontage = nullptr;
    ActiveRHBHMontageAttemptSerial = 0;
    if (FrameworkThrowComponent)
    {
        FrameworkThrowComponent->NotifyRecoveryComplete();
    }
}

void ADiscGolferPawn::HandleAnimatedThrowRecovered(int64 AttemptSerial, bool bDiscWasReleased)
{
    if (AttemptSerial <= 0 || AttemptSerial != ActiveRHBHMontageAttemptSerial)
    {
        return;
    }

    const bool bAuthoritativeLaunchAccepted = RHBHThrowAdapter
        && RHBHThrowAdapter->WasLastAuthoritativeLaunchAccepted();
    const bool bSuccessfulAuthoredFinish = RHBHThrowAdapter
        && RHBHThrowAdapter->GetRecoveryReason()
            == EDiscGolfRHBHThrowRecoveryReason::ThrowFinished
        && bDiscWasReleased
        && bAuthoritativeLaunchAccepted;

    // A pre-release cancellation (or a release-frame handoff rejected by the
    // gameplay authority) must also clear the legacy presentation/timing state.
    // Successful launches retain the existing release/follow-through feedback.
    if (!bSuccessfulAuthoredFinish)
    {
        CancelThrowPresentation();
        if (FrameworkThrowComponent)
        {
            FrameworkThrowComponent->CancelThrow();
        }
        StopAndClearActiveRHBHMontage(0.05f);
    }
    // A successful ThrowFinished notify releases gameplay ownership but leaves
    // the token and montage intact. The end delegate closes the presentation
    // only after the authored recovery/blend-out has played naturally.
}

FString ADiscGolferPawn::GetGolferPresentationStatusText() const
{
    return PresentationComponent ? PresentationComponent->GetStatusText() : TEXT("PRESENTATION UNAVAILABLE");
}

void ADiscGolferPawn::InputAim(const FInputActionValue& ActionValue)
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::AimThrow))
    {
        CancelThrowPresentation();
        return;
    }
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
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::AimThrow))
    {
        CancelThrowPresentation();
        return;
    }
    if (IsAnimatedThrowActive()) return;
    ThrowController->AdjustPower(ActionValue.Get<float>(), GetWorld()->GetDeltaSeconds());
    if (ThrowController->IsTimingActive()) RefreshPreCommitThrowPresentation();
}

void ADiscGolferPawn::InputHyzer(const FInputActionValue& ActionValue)
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::AimThrow))
    {
        CancelThrowPresentation();
        return;
    }
    if (IsAnimatedThrowActive()) return;
    ThrowController->AdjustHyzer(ActionValue.Get<float>(), GetWorld()->GetDeltaSeconds());
    if (ThrowController->IsTimingActive()) RefreshPreCommitThrowPresentation();
}

void ADiscGolferPawn::InputNose(const FInputActionValue& ActionValue)
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::AimThrow))
    {
        CancelThrowPresentation();
        return;
    }
    if (IsAnimatedThrowActive()) return;
    const ADiscGolfTourGameMode* GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    const float Invert = GameMode && GameMode->GetPlayerSettings().bInvertY ? -1.0f : 1.0f;
    ThrowController->AdjustNose(ActionValue.Get<float>() * Invert, GetWorld()->GetDeltaSeconds());
    if (ThrowController->IsTimingActive()) RefreshPreCommitThrowPresentation();
}

void ADiscGolferPawn::InputThrow()
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::AimThrow))
    {
        CancelThrowPresentation();
        return;
    }
    if (IsAnimatedThrowActive()) return;
    ADiscGolfTourGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    if (!GM || !GM->CanPlayerThrow())
    {
        CancelThrowPresentation();
        return;
    }

    const FDiscGolfPlayerSettings Settings = GM->GetPlayerSettings();
    ThrowController->SetAccessibilityAssist(Settings.AimAssist01, Settings.TimingWindowScale);
    const FVector AssistTargetDirection = GM->GetActiveHole()
        ? (GM->GetActiveHole()->BasketLocation - GetActorLocation()).GetSafeNormal()
        : GetActorForwardVector();

    FThrowCommand Command;
    if (ThrowController->HandleThrowPress(
        DiscBag->GetSelectedMoldId(), DiscBag->GetSelectedPlastic(),
        GetActorForwardVector(), AssistTargetDirection, Command))
    {
        // Snapshot player identity and stable equipment identity into the same
        // immutable command that the animation adapter and launch path verify.
        Command.Handedness = RuntimeCharacterProfile
            ? RuntimeCharacterProfile->Handedness
            : EDGHandedness::Right;
        Command.DiscInstanceId = DiscBag->GetSelectedDiscInstanceId();
        if (!RuntimeCharacterProfile
            || !Command.DiscInstanceId.IsValid()
            || !DiscGolfMath::IsThrowCommandValid(Command))
        {
            CancelThrowPresentation();
            return;
        }

        // The first press owns only presentation. Retire that aim/waggle state
        // before the adapter begins the tokenized committed transaction.
        EndPreCommitThrowPresentation();

        if (Command.ThrowStyle != EThrowStyle::Backhand
            || !TryStartAnimatedRHBHThrow(Command))
        {
            // Forehand, left-handed, and missing-family routes remain on the
            // calibrated synchronous path. Animation never becomes release
            // authority.
            if (!GM->RequestThrow(Command))
            {
                CancelThrowPresentation();
            }
        }
    }
    else if (PresentationComponent)
    {
        if (ThrowController->IsTimingActive())
        {
            RefreshPreCommitThrowPresentation();
            PresentationComponent->BeginTiming(ThrowController->GetThrowStyle());
        }
        else
        {
            CancelThrowPresentation();
        }
    }
}

void ADiscGolferPawn::InputToggleThrowStyle()
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::AimThrow))
    {
        CancelThrowPresentation();
        return;
    }
    if (IsAnimatedThrowActive()) return;
    ThrowController->ToggleThrowStyle();
    if (ThrowController->IsTimingActive()) RefreshPreCommitThrowPresentation();
}

void ADiscGolferPawn::InputResetHole()
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay)) return;
    CancelAnimatedThrow();
    CancelThrowPresentation();
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>())
    {
        GM->ResetHole();
    }
}

void ADiscGolferPawn::InputCyclePlastic()
{
    if (IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay) && !IsAnimatedThrowActive())
    {
        DiscBag->CyclePlastic();
    }
}
void ADiscGolferPawn::InputCycleRegressionPreset()
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay)) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->CyclePhysicsRegressionPreset();
}
void ADiscGolferPawn::InputRunRegressionPreset()
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay)) return;
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->RunSelectedPhysicsRegression();
}
void ADiscGolferPawn::InputRunRegressionSuite()
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay)) return;
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->RunPhysicsRegressionSuite();
}
void ADiscGolferPawn::InputToggleShotTracer()
{
    const EDiscGolfInputRoute Route = GetActiveInputRoute(this);
    const EDiscGolfInputRoute ActionRoute = Route == EDiscGolfInputRoute::Replay
        ? EDiscGolfInputRoute::Replay : EDiscGolfInputRoute::Gameplay;
    if (!IsRouteAllowed(this, ActionRoute)) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleShotTracer();
}
void ADiscGolferPawn::InputInstantReplay()
{
    const EDiscGolfInputRoute Route = GetActiveInputRoute(this);
    const EDiscGolfInputRoute ActionRoute = Route == EDiscGolfInputRoute::Replay
        ? EDiscGolfInputRoute::Replay : EDiscGolfInputRoute::Gameplay;
    if (!IsRouteAllowed(this, ActionRoute)) return;
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleInstantReplay();
}
void ADiscGolferPawn::InputToggleCourse()
{
    if (!IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay)) return;
    if (IsAnimatedThrowActive()) return;
    if (ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>()) GM->ToggleCourse();
}
void ADiscGolferPawn::InputCourseFlyover()
{
    ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>();
    const bool bUiPresentation = GM && (GM->IsHoleIntroVisible() || GM->IsCourseFlyoverActive());
    if (!IsRouteAllowed(this, bUiPresentation
        ? EDiscGolfInputRoute::UI : EDiscGolfInputRoute::Gameplay)) return;
    if (IsAnimatedThrowActive()) return;
    if (GM) GM->PreviewCourseFlyover();
}
void ADiscGolferPawn::InputNextHole()
{
    TryAdvanceToNextHoleFromPlayerInput();
}

bool ADiscGolferPawn::TryAdvanceToNextHoleFromPlayerInput()
{
    ADiscGolfTourGameMode* GM = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    if (!GM)
    {
        return false;
    }

    // A visible completed-hole scorecard owns the UI route, including the
    // advertised N / Right Trigger Next Hole or Restart Round action. Every
    // other next-hole request remains isolated to ordinary gameplay input.
    const bool bScorecardContinue = GM->IsScorecardVisible() && GM->IsHoleComplete();
    const EDiscGolfInputRoute RequiredRoute = bScorecardContinue
        ? EDiscGolfInputRoute::UI : EDiscGolfInputRoute::Gameplay;
    if (!IsRouteAllowed(this, RequiredRoute))
    {
        return false;
    }

    ADiscGolfHoleActor* PreviousHole = GM->GetActiveHole();
    const bool bWasRoundComplete = GM->IsRoundComplete();
    GM->AdvanceToNextHole();
    return GM->GetActiveHole() != PreviousHole
        || (bWasRoundComplete && !GM->IsRoundComplete());
}
void ADiscGolferPawn::InputScorecard()
{
    ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>();
    const bool bClosingScorecard = GM && GM->IsScorecardVisible();
    if (!IsRouteAllowed(this, bClosingScorecard
        ? EDiscGolfInputRoute::UI : EDiscGolfInputRoute::Gameplay)) return;
    if (IsAnimatedThrowActive()) return;
    if (GM) GM->ToggleScorecard();
}
void ADiscGolferPawn::InputDisc1() { if (IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay) && !IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(0); }
void ADiscGolferPawn::InputDisc2() { if (IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay) && !IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(1); }
void ADiscGolferPawn::InputDisc3() { if (IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay) && !IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(2); }
void ADiscGolferPawn::InputDisc4() { if (IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay) && !IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(3); }
void ADiscGolferPawn::InputDisc5() { if (IsRouteAllowed(this, EDiscGolfInputRoute::Gameplay) && !IsAnimatedThrowActive()) DiscBag->SelectDiscIndex(4); }
