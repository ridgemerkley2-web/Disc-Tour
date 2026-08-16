#include "DiscGolfSession4VisualCaptureRunner.h"

#include "DiscActor.h"
#include "DiscGolferPawn.h"
#include "DiscGolfCharacterCreatorWidget.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

namespace DiscGolfSession4VisualCapture
{
constexpr double RuntimeTimeoutSeconds = 8.0;
constexpr double PoseTimeoutSeconds = 7.0;
constexpr double Session4ScreenshotTimeoutSeconds = 30.0;
constexpr double PoseSettleSeconds = 0.30;
constexpr float ReachbackSeconds = 54.0f / 60.0f;
constexpr float PlantSeconds = 70.0f / 60.0f;
constexpr float ReleaseSeconds = 96.0f / 60.0f;
constexpr float FollowThroughSeconds = 112.0f / 60.0f;
constexpr int32 ExpectedWidth = 1920;
constexpr int32 ExpectedHeight = 1080;
constexpr int64 MinimumReadablePngBytes = 32768;

const TCHAR* Session4CaptureFilenames[] = {
    TEXT("01_Neutral_Front_Short_Baseline_Tall.png"),
    TEXT("02_Neutral_Side_Short_Baseline_Tall.png"),
    TEXT("03_Maximum_Reachback_AllProfiles.png"),
    TEXT("04_Plant_Brace_AllProfiles.png"),
    TEXT("05_Release_AllProfiles.png"),
    TEXT("06_FollowThrough_AllProfiles.png"),
    TEXT("07_Live_CharacterCreator_ShortPreview.png"),
    TEXT("08_Slider_Extremes_Min_Max.png")
};

const TCHAR* Session4ProfileNames[] = {
    TEXT("ShortCompact"),
    TEXT("Baseline"),
    TEXT("TallLongArms")
};

const TCHAR* Session4ProfilePaths[] = {
    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact"),
    TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"),
    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms")
};

struct FRequiredAsset
{
    const TCHAR* Label;
    const TCHAR* Path;
};

const FRequiredAsset RequiredAssetPaths[] = {
    {TEXT("skeletal_mesh"), TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master")},
    {TEXT("skeleton"), TEXT("/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master")},
    {TEXT("ik_rig"), TEXT("/Game/DiscGolf/Rigs/IK_DG_Master.IK_DG_Master")},
    {TEXT("control_rig"), TEXT("/Game/DiscGolf/Rigs/CR_DG_Master.CR_DG_Master")},
    {TEXT("animation_blueprint"), TEXT("/Game/DiscGolf/Animation/ABP_DG_Player.ABP_DG_Player")},
    {TEXT("montage"), TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype")}
};

bool IsSession4FiniteVector(const FVector& Value)
{
    return !Value.ContainsNaN()
        && FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

bool NearlyEqualBody(const FDGBodyProfile& A, const FDGBodyProfile& B)
{
    return FMath::IsNearlyEqual(A.HeightCm, B.HeightCm)
        && FMath::IsNearlyEqual(A.WingspanScale, B.WingspanScale)
        && FMath::IsNearlyEqual(A.ShoulderWidthScale, B.ShoulderWidthScale)
        && FMath::IsNearlyEqual(A.TorsoLengthScale, B.TorsoLengthScale)
        && FMath::IsNearlyEqual(A.LegLengthScale, B.LegLengthScale)
        && FMath::IsNearlyEqual(A.HandScale, B.HandScale)
        && FMath::IsNearlyEqual(A.MassKg, B.MassKg);
}

bool NearlyEqualStyle(const FDGThrowStyle& A, const FDGThrowStyle& B)
{
    return FMath::IsNearlyEqual(A.RunUpIntensity, B.RunUpIntensity)
        && FMath::IsNearlyEqual(A.ReachBackAmount, B.ReachBackAmount)
        && FMath::IsNearlyEqual(A.TorsoRotation, B.TorsoRotation)
        && FMath::IsNearlyEqual(A.BraceIntensity, B.BraceIntensity)
        && FMath::IsNearlyEqual(A.Explosiveness, B.Explosiveness)
        && FMath::IsNearlyEqual(A.FollowThrough, B.FollowThrough)
        && FMath::IsNearlyEqual(A.PowerMultiplier, B.PowerMultiplier)
        && FMath::IsNearlyEqual(A.SpinMultiplier, B.SpinMultiplier);
}

int32 ReadSession4BigEndianInt32(const uint8* Bytes)
{
    return (static_cast<int32>(Bytes[0]) << 24)
        | (static_cast<int32>(Bytes[1]) << 16)
        | (static_cast<int32>(Bytes[2]) << 8)
        | static_cast<int32>(Bytes[3]);
}

TSharedRef<FJsonObject> BodyToJson(const FDGBodyProfile& Body)
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("height_cm"), Body.HeightCm);
    Json->SetNumberField(TEXT("wingspan_scale"), Body.WingspanScale);
    Json->SetNumberField(TEXT("shoulder_width_scale"), Body.ShoulderWidthScale);
    Json->SetNumberField(TEXT("torso_length_scale"), Body.TorsoLengthScale);
    Json->SetNumberField(TEXT("leg_length_scale"), Body.LegLengthScale);
    Json->SetNumberField(TEXT("hand_scale"), Body.HandScale);
    Json->SetNumberField(TEXT("mass_kg"), Body.MassKg);
    return Json;
}

TSharedRef<FJsonObject> StyleToJson(const FDGThrowStyle& Style)
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("run_up_intensity"), Style.RunUpIntensity);
    Json->SetNumberField(TEXT("reach_back_amount"), Style.ReachBackAmount);
    Json->SetNumberField(TEXT("torso_rotation"), Style.TorsoRotation);
    Json->SetNumberField(TEXT("brace_intensity"), Style.BraceIntensity);
    Json->SetNumberField(TEXT("explosiveness"), Style.Explosiveness);
    Json->SetNumberField(TEXT("follow_through"), Style.FollowThrough);
    Json->SetNumberField(TEXT("power_multiplier"), Style.PowerMultiplier);
    Json->SetNumberField(TEXT("spin_multiplier"), Style.SpinMultiplier);
    return Json;
}
}

using namespace DiscGolfSession4VisualCapture;

ADiscGolfSession4VisualCaptureRunner::ADiscGolfSession4VisualCaptureRunner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ADiscGolfSession4VisualCaptureRunner::Start()
{
    if (bStarted || bFinished)
    {
        return;
    }
    bStarted = true;

    GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    PossessedGolfer = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0));
    ThrowMontage = LoadObject<UAnimMontage>(
        nullptr,
        TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype"));

    if (!GameMode || !PlayerController || !PossessedGolfer || !ThrowMontage
        || !PossessedGolfer->GetSkeletalGolferMesh())
    {
        Fail(TEXT("game mode, local controller, possessed golfer, mesh, or montage was unavailable"));
        return;
    }

    OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("CharacterFramework/Screenshots/Session4_CharacterCreator"));
    ManifestPath = FPaths::Combine(
        OutputDirectory,
        TEXT("Session4_CharacterCreator_CaptureManifest.json"));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    for (const TCHAR* Filename : Session4CaptureFilenames)
    {
        IFileManager::Get().Delete(
            *FPaths::Combine(OutputDirectory, Filename), false, true, true);
    }
    IFileManager::Get().Delete(*ManifestPath, false, true, true);

    SnapshotPersistentFiles(FPaths::ProjectContentDir(), true, InitialPackageFiles);
    SnapshotPersistentFiles(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")),
        false,
        InitialSaveGameFiles);

    GameMode->SkipCurrentPresentation();
    InitialGolferTransform = PossessedGolfer->GetActorTransform();
    FixtureStageTransform = InitialGolferTransform;
    // Keep rendered proxy evidence clear of the production course canopy and
    // terrain. This is a transient validation stage only; the possessed pawn,
    // gameplay world, and all persistent assets remain at their real transforms.
    // Ten kilometres clears the course canopy while remaining inside the
    // presentation sky shell; its outer surface sits near the previous 25 km
    // test height and can otherwise occlude phase captures.
    FixtureStageTransform.AddToTranslation(FVector(0.0f, 0.0f, 10000.0f));
    InitialViewTarget = PlayerController->GetViewTarget();
    bInitialGolferHidden = PossessedGolfer->IsHidden();
    BaselineWorldDiscCount = CountWorldDiscs();
    BaselineStrokes = GameMode->GetStrokes();
    if (!PossessedGolfer->GetCharacterCreatorProfile(
            CreatorOpeningBody,
            CreatorOpeningThrowStyle,
            CreatorOpeningHandedness))
    {
        Fail(TEXT("possessed golfer profile could not be snapshotted"));
        return;
    }

    if (AHUD* Hud = PlayerController->GetHUD())
    {
        bInitialHudVisible = Hud->bShowHUD;
        Hud->bShowHUD = false;
    }
    PossessedGolfer->SetActorHiddenInGame(true);

    FixtureThrowCommand.MoldId = TEXT("Session4VisualEvidence");
    FixtureThrowCommand.ThrowStyle = EThrowStyle::Backhand;
    FixtureThrowCommand.ShotContext = EDiscShotContext::Drive;
    FixtureThrowCommand.Direction = InitialGolferTransform.GetRotation().GetForwardVector();
    FixtureThrowCommand.Power01 = 0.82f;
    FixtureThrowCommand.HyzerDeg = 3.0f;
    FixtureThrowCommand.NoseAngleDeg = 1.0f;
    FixtureThrowCommand.LaunchAngleDeg = 7.0f;
    FixtureThrowCommand.TimingError = 0.0f;

    if (!PreflightAssets() || !CreateCaptureCamera() || !SpawnRegularFixtures()
        || !RunHandednessBoundaryAssertion())
    {
        if (!bFinished)
        {
            Fail(TEXT("Session 4 visual preflight failed"));
        }
        return;
    }

    ArrangeFixturesFront();
    SetStage(EStage::WaitingForNeutralFront);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION4_VISUAL_CAPTURE: START output=%s"), *OutputDirectory);
}

void ADiscGolfSession4VisualCaptureRunner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bStarted || bFinished)
    {
        return;
    }
    if (!GameMode || !PlayerController || !PossessedGolfer)
    {
        Fail(TEXT("runtime authority disappeared during capture"));
        return;
    }
    if (!ValidateNoGameplayMutation())
    {
        Fail(TEXT("a visual fixture changed gameplay disc or stroke authority"));
        return;
    }
    if (bScreenshotPending)
    {
        if (bPendingUsesCaptureCamera && CaptureCamera)
        {
            PlayerController->SetViewTarget(CaptureCamera);
            if (PlayerController->PlayerCameraManager)
            {
                PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
            }
        }
        PollPendingCapture();
        return;
    }

    switch (Stage)
    {
        case EStage::WaitingForNeutralFront:
            if (SecondsInStage() >= PoseSettleSeconds)
            {
                if (!RecordNeutralMetrics())
                {
                    Fail(TEXT("neutral body measurements were invalid"));
                    return;
                }
                PrepareNeutralFrontCapture();
            }
            else if (SecondsInStage() > RuntimeTimeoutSeconds)
            {
                Fail(TEXT("neutral front fixtures did not settle"));
            }
            break;

        case EStage::WaitingForNeutralSide:
            if (SecondsInStage() >= PoseSettleSeconds)
            {
                PrepareNeutralSideCapture();
            }
            break;

        case EStage::WaitingForReachback:
            if (AreFixturesAtMontageTime(ReachbackSeconds))
            {
                PrepareReachbackCapture();
            }
            else if (SecondsInStage() > PoseTimeoutSeconds)
            {
                Fail(TEXT("maximum reachback pose was not reached"));
            }
            break;

        case EStage::WaitingForPlant:
            if (AreFixturesAtMontageTime(PlantSeconds))
            {
                PreparePlantCapture();
            }
            else if (SecondsInStage() > PoseTimeoutSeconds)
            {
                Fail(TEXT("plant/brace pose was not reached"));
            }
            break;

        case EStage::WaitingForRelease:
            if (AreFixturesAtMontageTime(ReleaseSeconds)
                && FixtureLaunchCount == FixturePawns.Num())
            {
                PrepareReleaseCapture();
            }
            else if (SecondsInStage() > PoseTimeoutSeconds)
            {
                Fail(TEXT("release pose or one-shot release gate was not reached"));
            }
            break;

        case EStage::WaitingForFollowThrough:
            if (AreFixturesAtMontageTime(FollowThroughSeconds))
            {
                PrepareFollowThroughCapture();
            }
            else if (SecondsInStage() > PoseTimeoutSeconds)
            {
                Fail(TEXT("follow-through pose was not reached"));
            }
            break;

        case EStage::WaitingForCreator:
            if (!bCreatorOpened)
            {
                if (!PrepareCreatorCapture())
                {
                    Fail(TEXT("live character creator could not open or apply its evidence draft"));
                }
                else
                {
                    SetStage(EStage::WaitingForCreator);
                }
            }
            else if (SecondsInStage() >= PoseSettleSeconds)
            {
                PrepareCreatorCapture();
            }
            else if (SecondsInStage() > RuntimeTimeoutSeconds)
            {
                Fail(TEXT("live character creator did not become readable"));
            }
            break;

        case EStage::WaitingForExtremes:
            if (SecondsInStage() >= PoseSettleSeconds)
            {
                if (!RecordNeutralMetrics())
                {
                    Fail(TEXT("slider-extreme body measurements were invalid"));
                    return;
                }
                PrepareExtremeCapture();
            }
            break;

        case EStage::WaitingForRuntime:
            if (SecondsInStage() > RuntimeTimeoutSeconds)
            {
                Fail(TEXT("visual runner was not initialized"));
            }
            break;

        case EStage::Capturing:
        case EStage::Finished:
        default:
            break;
    }
}

void ADiscGolfSession4VisualCaptureRunner::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    RestoreRuntimeState();
    Super::EndPlay(EndPlayReason);
}

void ADiscGolfSession4VisualCaptureRunner::SetStage(EStage NewStage)
{
    Stage = NewStage;
    StageStartRealSeconds = FPlatformTime::Seconds();
}

double ADiscGolfSession4VisualCaptureRunner::SecondsInStage() const
{
    return FPlatformTime::Seconds() - StageStartRealSeconds;
}

bool ADiscGolfSession4VisualCaptureRunner::PreflightAssets()
{
    RequiredAssets.Reset();
    for (const FRequiredAsset& Required : RequiredAssetPaths)
    {
        UObject* Asset = LoadObject<UObject>(nullptr, Required.Path);
        if (!Asset)
        {
            Fail(FString::Printf(
                TEXT("required %s asset did not load: %s"),
                Required.Label,
                Required.Path));
            return false;
        }
        RequiredAssets.Add(Asset);
    }
    for (const TCHAR* ProfilePath : Session4ProfilePaths)
    {
        UDiscGolfCharacterProfile* Profile =
            LoadObject<UDiscGolfCharacterProfile>(nullptr, ProfilePath);
        if (!Profile)
        {
            Fail(FString::Printf(TEXT("profile asset did not load: %s"), ProfilePath));
            return false;
        }
        RequiredAssets.Add(Profile);
    }
    return true;
}

bool ADiscGolfSession4VisualCaptureRunner::CreateCaptureCamera()
{
    CaptureCamera = GetWorld()->SpawnActor<ACameraActor>();
    if (!CaptureCamera)
    {
        return false;
    }
    CaptureCamera->SetActorEnableCollision(false);

    const auto CreateLight = [this](
        const TCHAR* Name,
        const FVector& RelativeLocation,
        float Intensity,
        const FColor& Color)
    {
        UPointLightComponent* Light = NewObject<UPointLightComponent>(CaptureCamera, Name);
        if (!Light)
        {
            return static_cast<UPointLightComponent*>(nullptr);
        }
        CaptureCamera->AddInstanceComponent(Light);
        Light->SetupAttachment(CaptureCamera->GetRootComponent());
        Light->SetRelativeLocation(RelativeLocation);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetIntensityUnits(ELightUnits::Lumens);
        Light->SetIntensity(Intensity);
        Light->SetAttenuationRadius(3000.0f);
        Light->SetLightColor(Color);
        Light->SetCastShadows(false);
        Light->SetVolumetricScatteringIntensity(0.0f);
        Light->RegisterComponent();
        return Light;
    };

    CaptureKeyLight = CreateLight(
        TEXT("Session4CaptureKey"),
        FVector(250.0f, -220.0f, 250.0f),
        5000.0f,
        FColor(255, 244, 226));
    CaptureFillLight = CreateLight(
        TEXT("Session4CaptureFill"),
        FVector(120.0f, 240.0f, 100.0f),
        1600.0f,
        FColor(205, 224, 255));
    return CaptureKeyLight && CaptureFillLight;
}

bool ADiscGolfSession4VisualCaptureRunner::SpawnRegularFixtures()
{
    DestroyFixtures();
    FixtureLaunchCount = 0;
    FixtureLaunchGripTransforms.Reset();

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Session4ProfilePaths); ++Index)
    {
        UDiscGolfCharacterProfile* Profile =
            LoadObject<UDiscGolfCharacterProfile>(nullptr, Session4ProfilePaths[Index]);
        if (!Profile || !SpawnFixture(
                Session4ProfileNames[Index],
                Session4ProfilePaths[Index],
                TEXT("AUTHORED_PROFILE"),
                Profile->Body,
                Profile->ThrowStyle,
                Profile->Handedness,
                FixtureStageTransform))
        {
            return false;
        }
    }
    return FixturePawns.Num() == 3;
}

bool ADiscGolfSession4VisualCaptureRunner::SpawnExtremeFixtures()
{
    DestroyFixtures();

    FDiscGolfCharacterProfileSaveData Minimum;
    Minimum.HeightCm = DiscGolfCharacterCreatorSchema::MinHeightCm;
    Minimum.WingspanScale = DiscGolfCharacterCreatorSchema::MinWingspanScale;
    Minimum.ShoulderWidthScale = DiscGolfCharacterCreatorSchema::MinShoulderWidthScale;
    Minimum.TorsoLengthScale = DiscGolfCharacterCreatorSchema::MinTorsoLengthScale;
    Minimum.LegLengthScale = DiscGolfCharacterCreatorSchema::MinLegLengthScale;
    Minimum.HandScale = DiscGolfCharacterCreatorSchema::MinHandScale;
    Minimum.MassKg = DiscGolfCharacterCreatorSchema::MinMassKg;
    Minimum.RunUpIntensity = 0.0f;
    Minimum.ReachBackAmount = 0.0f;
    Minimum.TorsoRotation = 0.0f;
    Minimum.BraceIntensity = 0.0f;
    Minimum.Explosiveness = 0.0f;
    Minimum.FollowThrough = 0.0f;
    Minimum.Sanitize();

    FDiscGolfCharacterProfileSaveData Maximum;
    Maximum.HeightCm = DiscGolfCharacterCreatorSchema::MaxHeightCm;
    Maximum.WingspanScale = DiscGolfCharacterCreatorSchema::MaxWingspanScale;
    Maximum.ShoulderWidthScale = DiscGolfCharacterCreatorSchema::MaxShoulderWidthScale;
    Maximum.TorsoLengthScale = DiscGolfCharacterCreatorSchema::MaxTorsoLengthScale;
    Maximum.LegLengthScale = DiscGolfCharacterCreatorSchema::MaxLegLengthScale;
    Maximum.HandScale = DiscGolfCharacterCreatorSchema::MaxHandScale;
    Maximum.MassKg = DiscGolfCharacterCreatorSchema::MaxMassKg;
    Maximum.RunUpIntensity = 1.0f;
    Maximum.ReachBackAmount = 1.0f;
    Maximum.TorsoRotation = 1.0f;
    Maximum.BraceIntensity = 1.0f;
    Maximum.Explosiveness = 1.0f;
    Maximum.FollowThrough = 1.0f;
    Maximum.Sanitize();

    const bool bMin = SpawnFixture(
        TEXT("SliderMin"),
        TEXT("TRANSIENT_SCHEMA_MINIMUM_NO_ASSET"),
        TEXT("TRANSIENT_SLIDER_EXTREME"),
        Minimum.ToBodyProfile(),
        Minimum.ToThrowStyle(),
        Minimum.GetHandedness(),
        FixtureStageTransform);
    const bool bMax = SpawnFixture(
        TEXT("SliderMax"),
        TEXT("TRANSIENT_SCHEMA_MAXIMUM_NO_ASSET"),
        TEXT("TRANSIENT_SLIDER_EXTREME"),
        Maximum.ToBodyProfile(),
        Maximum.ToThrowStyle(),
        Maximum.GetHandedness(),
        FixtureStageTransform);
    return bMin && bMax && FixturePawns.Num() == 2;
}

bool ADiscGolfSession4VisualCaptureRunner::SpawnFixture(
    const FString& Name,
    const FString& AssetPath,
    const FString& FixtureKind,
    const FDGBodyProfile& Body,
    const FDGThrowStyle& ThrowStyle,
    EDGHandedness Handedness,
    const FTransform& Transform)
{
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ADiscGolferPawn* Fixture = GetWorld()->SpawnActor<ADiscGolferPawn>(
        ADiscGolferPawn::StaticClass(), Transform, SpawnParameters);
    UDiscGolfRHBHThrowAdapterComponent* Adapter = Fixture
        ? Fixture->GetRHBHThrowAdapter() : nullptr;
    USkeletalMeshComponent* Mesh = Fixture
        ? Fixture->GetSkeletalGolferMesh() : nullptr;
    if (!Fixture || !Adapter || !Mesh || !Mesh->GetSkeletalMeshAsset()
        || !Fixture->GetRuntimeCharacterProfile())
    {
        if (Fixture)
        {
            Fixture->Destroy();
        }
        return false;
    }

    Fixture->SetActorEnableCollision(false);
    if (!Fixture->PreviewCharacterCreatorProfile(Body, ThrowStyle, Handedness))
    {
        Fixture->Destroy();
        return false;
    }
    Adapter->GetAuthoritativeLaunchDelegate().Unbind();
    Adapter->GetAuthoritativeLaunchDelegate().BindUObject(
        this,
        &ADiscGolfSession4VisualCaptureRunner::HandleFixtureLaunch);

    const int32 ColorIndex = FixturePawns.Num();
    const FLinearColor Colors[] = {
        FLinearColor(0.05f, 0.28f, 0.72f, 1.0f),
        FLinearColor(0.18f, 0.20f, 0.23f, 1.0f),
        FLinearColor(0.78f, 0.37f, 0.04f, 1.0f)
    };
    ApplyEvidenceMaterial(Mesh, Colors[FMath::Clamp(ColorIndex, 0, 2)]);
    Mesh->RefreshBoneTransforms();
    Mesh->UpdateBounds();

    FProfileEvidence Evidence;
    Evidence.Name = Name;
    Evidence.AssetPath = AssetPath;
    Evidence.FixtureKind = FixtureKind;
    Evidence.Body = Body;
    Evidence.ThrowStyle = ThrowStyle;
    Evidence.Handedness = Handedness;
    Evidence.bSameSkeletalMesh =
        Mesh->GetSkeletalMeshAsset()->GetPathName()
        == TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master");
    Evidence.bSameSkeleton = Mesh->GetSkeletalMeshAsset()->GetSkeleton()
        && Mesh->GetSkeletalMeshAsset()->GetSkeleton()->GetPathName()
        == TEXT("/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master");
    Evidence.bSameAnimBlueprint = Mesh->GetAnimInstance()
        && Mesh->GetAnimInstance()->GetClass()->GetPathName().StartsWith(
            TEXT("/Game/DiscGolf/Animation/ABP_DG_Player.ABP_DG_Player_C"));

    const int32 EvidenceIndex = ProfileEvidence.Add(Evidence);
    ActiveProfileEvidenceIndices.Add(EvidenceIndex);
    FixturePawns.Add(Fixture);
    FixtureAdapters.Add(Adapter);
    FixtureMeshes.Add(Mesh);
    return Evidence.bSameSkeletalMesh
        && Evidence.bSameSkeleton
        && Evidence.bSameAnimBlueprint;
}

void ADiscGolfSession4VisualCaptureRunner::DestroyFixtures()
{
    for (UDiscGolfRHBHThrowAdapterComponent* Adapter : FixtureAdapters)
    {
        if (Adapter)
        {
            Adapter->GetAuthoritativeLaunchDelegate().Unbind();
        }
    }
    for (ADiscGolferPawn* Fixture : FixturePawns)
    {
        if (IsValid(Fixture))
        {
            Fixture->Destroy();
        }
    }
    FixturePawns.Reset();
    FixtureAdapters.Reset();
    FixtureMeshes.Reset();
    ActiveProfileEvidenceIndices.Reset();
}

void ADiscGolfSession4VisualCaptureRunner::ArrangeFixturesFront()
{
    const FVector Anchor = FixtureStageTransform.GetLocation();
    const FVector Right = FixtureStageTransform.GetRotation().GetRightVector();
    const int32 Count = FixturePawns.Num();
    const float Spacing = Count == 2 ? 300.0f : 205.0f;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float CenteredIndex = static_cast<float>(Index)
            - static_cast<float>(Count - 1) * 0.5f;
        FixturePawns[Index]->SetActorLocationAndRotation(
            Anchor + Right * (CenteredIndex * Spacing),
            FixtureStageTransform.Rotator());
        FixtureMeshes[Index]->RefreshBoneTransforms();
        FixtureMeshes[Index]->UpdateBounds();
    }
}

void ADiscGolfSession4VisualCaptureRunner::ArrangeFixturesSide()
{
    const FVector Anchor = FixtureStageTransform.GetLocation();
    const FVector Forward = FixtureStageTransform.GetRotation().GetForwardVector();
    const int32 Count = FixturePawns.Num();
    const float Spacing = 205.0f;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float CenteredIndex = static_cast<float>(Index)
            - static_cast<float>(Count - 1) * 0.5f;
        FixturePawns[Index]->SetActorLocationAndRotation(
            Anchor + Forward * (CenteredIndex * Spacing),
            FixtureStageTransform.Rotator());
        FixtureMeshes[Index]->RefreshBoneTransforms();
        FixtureMeshes[Index]->UpdateBounds();
    }
}

bool ADiscGolfSession4VisualCaptureRunner::RunHandednessBoundaryAssertion()
{
    if (!FixturePawns.IsValidIndex(1) || !FixtureAdapters.IsValidIndex(1))
    {
        return false;
    }
    UDiscGolfCharacterProfile* BaselineAsset =
        LoadObject<UDiscGolfCharacterProfile>(nullptr, Session4ProfilePaths[1]);
    if (!BaselineAsset)
    {
        return false;
    }
    const FDGBodyProfile AssetBodyBefore = BaselineAsset->Body;
    const FDGThrowStyle AssetStyleBefore = BaselineAsset->ThrowStyle;
    const EDGHandedness AssetHandBefore = BaselineAsset->Handedness;
    const int32 DiscCountBefore = CountWorldDiscs();
    const int32 StrokeCountBefore = GameMode->GetStrokes();

    ADiscGolferPawn* Fixture = FixturePawns[1];
    UDiscGolfRHBHThrowAdapterComponent* Adapter = FixtureAdapters[1];
    if (!Fixture->PreviewCharacterCreatorProfile(
            AssetBodyBefore,
            AssetStyleBefore,
            EDGHandedness::Left))
    {
        return false;
    }
    const bool bUnexpectedStart = Fixture->TryStartAnimatedRHBHThrow(FixtureThrowCommand);
    bHandednessRejectedBeforeMontage = !bUnexpectedStart
        && !Adapter->IsThrowActive()
        && Adapter->GetReleaseCommitCountForAttempt() == 0
        && FixtureLaunchCount == 0;
    const bool bRestoredRight = Fixture->PreviewCharacterCreatorProfile(
        AssetBodyBefore,
        AssetStyleBefore,
        EDGHandedness::Right);
    bHandednessProfileAssetUnchanged =
        NearlyEqualBody(BaselineAsset->Body, AssetBodyBefore)
        && NearlyEqualStyle(BaselineAsset->ThrowStyle, AssetStyleBefore)
        && BaselineAsset->Handedness == AssetHandBefore;
    bHandednessBoundaryPassed = bHandednessRejectedBeforeMontage
        && bRestoredRight
        && bHandednessProfileAssetUnchanged
        && CountWorldDiscs() == DiscCountBefore
        && GameMode->GetStrokes() == StrokeCountBefore;
    return bHandednessBoundaryPassed;
}

bool ADiscGolfSession4VisualCaptureRunner::BeginFixtureThrows()
{
    FixtureLaunchCount = 0;
    FixtureLaunchGripTransforms.Reset();
    for (int32 Index = 0; Index < FixturePawns.Num(); ++Index)
    {
        if (!FixturePawns[Index] || !FixtureAdapters[Index]
            || FixtureAdapters[Index]->IsThrowActive()
            || !FixturePawns[Index]->TryStartAnimatedRHBHThrow(FixtureThrowCommand))
        {
            return false;
        }
        if (UAnimInstance* Anim = FixtureMeshes[Index]
                ? FixtureMeshes[Index]->GetAnimInstance() : nullptr)
        {
            // Keep the accepted authored rate. Screenshot requests pause the
            // montage while the adapter watchdog continues in world time, so
            // slow-motion evidence playback can legitimately recover before
            // the follow-through sample is reached.
            Anim->Montage_SetPlayRate(ThrowMontage, 1.0f);
        }
    }
    return true;
}

bool ADiscGolfSession4VisualCaptureRunner::HandleFixtureLaunch(
    const FThrowCommand& AuthoritativeCommand,
    const FTransform& GripWorldTransform)
{
    ++FixtureLaunchCount;
    FixtureLaunchGripTransforms.Add(GripWorldTransform);
    return AuthoritativeCommand.ThrowStyle == EThrowStyle::Backhand
        && AuthoritativeCommand.ShotContext == EDiscShotContext::Drive
        && GripWorldTransform.IsValid()
        && IsSession4FiniteVector(GripWorldTransform.GetLocation())
        && ValidateNoGameplayMutation();
}

bool ADiscGolfSession4VisualCaptureRunner::AreFixturesAtMontageTime(
    float TargetSeconds) const
{
    if (FixtureMeshes.IsEmpty())
    {
        return false;
    }
    for (const USkeletalMeshComponent* Mesh : FixtureMeshes)
    {
        if (GetMontagePosition(Mesh) < TargetSeconds)
        {
            return false;
        }
    }
    return true;
}

void ADiscGolfSession4VisualCaptureRunner::PauseFixtureMontages(bool bPause) const
{
    for (USkeletalMeshComponent* Mesh : FixtureMeshes)
    {
        UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
        if (!Anim)
        {
            continue;
        }
        if (bPause)
        {
            Anim->Montage_Pause(ThrowMontage);
        }
        else
        {
            Anim->Montage_Resume(ThrowMontage);
        }
    }
}

float ADiscGolfSession4VisualCaptureRunner::GetMontagePosition(
    const USkeletalMeshComponent* Mesh) const
{
    const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    return Anim && ThrowMontage ? Anim->Montage_GetPosition(ThrowMontage) : -1.0f;
}

float ADiscGolfSession4VisualCaptureRunner::GetMinimumCurveValue(FName CurveName) const
{
    float Minimum = TNumericLimits<float>::Max();
    for (const USkeletalMeshComponent* Mesh : FixtureMeshes)
    {
        const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
        if (!Anim)
        {
            return -1.0f;
        }
        Minimum = FMath::Min(Minimum, Anim->GetCurveValue(CurveName));
    }
    return Minimum == TNumericLimits<float>::Max() ? -1.0f : Minimum;
}

bool ADiscGolfSession4VisualCaptureRunner::ValidatePhaseGate(
    FName CurveName,
    float MinimumValue)
{
    MinimumPhaseCurveValue = GetMinimumCurveValue(CurveName);
    return FMath::IsFinite(MinimumPhaseCurveValue)
        && MinimumPhaseCurveValue >= MinimumValue;
}

bool ADiscGolfSession4VisualCaptureRunner::ValidatePlantContactGate()
{
    if (!ValidatePhaseGate(TEXT("DG_BraceAlpha"), 0.85f)
        || GetMinimumCurveValue(TEXT("DG_FootPlant_L")) < 0.85f)
    {
        return false;
    }
    for (int32 Index = 0; Index < FixtureMeshes.Num(); ++Index)
    {
        if (!ProfileEvidence.IsValidIndex(ActiveProfileEvidenceIndices[Index]))
        {
            return false;
        }
        const float CurrentLeftFootZ =
            GetBoneLocation(FixtureMeshes[Index], TEXT("foot_l")).Z;
        const float NeutralLeftFootZ =
            ProfileEvidence[ActiveProfileEvidenceIndices[Index]].NeutralLeftFootZ;
        if (!FMath::IsFinite(CurrentLeftFootZ)
            || FMath::Abs(CurrentLeftFootZ - NeutralLeftFootZ) > 25.0f)
        {
            return false;
        }
    }
    return true;
}

bool ADiscGolfSession4VisualCaptureRunner::ValidateReleaseGate()
{
    if (!ValidatePhaseGate(TEXT("DG_ReleaseApproachAlpha"), 0.60f)
        || FixtureLaunchCount != FixturePawns.Num()
        || FixtureLaunchGripTransforms.Num() != FixturePawns.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < FixtureAdapters.Num(); ++Index)
    {
        const UDiscGolfRHBHThrowAdapterComponent* Adapter = FixtureAdapters[Index];
        if (!Adapter || !Adapter->HasCommittedRelease()
            || Adapter->GetReleaseCommitCountForAttempt() != 1
            || GetMontagePosition(FixtureMeshes[Index]) > ReleaseSeconds + 0.07f
            || !FixtureLaunchGripTransforms[Index].IsValid()
            || !IsSession4FiniteVector(FixtureLaunchGripTransforms[Index].GetLocation()))
        {
            return false;
        }
        if (ProfileEvidence.IsValidIndex(ActiveProfileEvidenceIndices[Index]))
        {
            ProfileEvidence[ActiveProfileEvidenceIndices[Index]].ReleaseCommitCount = 1;
        }
    }
    return ValidateNoGameplayMutation();
}

bool ADiscGolfSession4VisualCaptureRunner::ValidateNoGameplayMutation() const
{
    return GameMode
        && CountWorldDiscs() == BaselineWorldDiscCount
        && GameMode->GetStrokes() == BaselineStrokes;
}

void ADiscGolfSession4VisualCaptureRunner::PrepareNeutralFrontCapture()
{
    MinimumPhaseCurveValue = -1.0f;
    PositionCompositeCamera(false);
    ShowEvidenceLabel(
        TEXT("SESSION 4 | NEUTRAL FRONT COMPARISON"),
        TEXT("LEFT TALL / LONG ARMS  |  CENTER BASELINE  |  RIGHT SHORT COMPACT"));
    RequestCapture(
        0,
        TEXT("ShortCompact, Baseline, and TallLongArms neutral silhouettes from the front."),
        TEXT("Neutral"),
        TEXT("Front composite"),
        EStage::WaitingForNeutralSide);
    ValidateCompositeFraming(0);
}

void ADiscGolfSession4VisualCaptureRunner::PrepareNeutralSideCapture()
{
    MinimumPhaseCurveValue = -1.0f;
    PositionCompositeCamera(true);
    ShowEvidenceLabel(
        TEXT("SESSION 4 | NEUTRAL SIDE COMPARISON"),
        TEXT("SAME SK / SKEL / IK / CR / ABP  |  THREE PROFILE SILHOUETTES"));
    RequestCapture(
        1,
        TEXT("ShortCompact, Baseline, and TallLongArms neutral silhouettes from the side."),
        TEXT("Neutral"),
        TEXT("Side composite"),
        EStage::WaitingForReachback);
    ValidateCompositeFraming(1);
}

void ADiscGolfSession4VisualCaptureRunner::PrepareReachbackCapture()
{
    PauseFixtureMontages(true);
    if (!ValidatePhaseGate(TEXT("DG_ReachbackAlpha"), 0.90f))
    {
        Fail(TEXT("maximum-reachback curve gate was below 0.90"));
        return;
    }
    PositionCompositeCamera(false);
    ShowEvidenceLabel(
        TEXT("SESSION 4 | MAXIMUM REACHBACK"),
        FString::Printf(
            TEXT("ALL THREE PROFILES  |  DG_ReachbackAlpha MIN %.2f  |  NO GAMEPLAY DISCS"),
            MinimumPhaseCurveValue));
    RequestCapture(
        2,
        TEXT("All three profiles at the authored maximum-reachback phase."),
        TEXT("ReachBack"),
        TEXT("Three-quarter front composite"),
        EStage::WaitingForPlant);
    ValidateCompositeFraming(2);
}

void ADiscGolfSession4VisualCaptureRunner::PreparePlantCapture()
{
    PauseFixtureMontages(true);
    const bool bContactPassed = ValidatePlantContactGate();
    if (!bContactPassed)
    {
        Fail(TEXT("plant/brace curve or left-foot contact gate failed"));
        return;
    }
    PositionCompositeCamera(false);
    ShowEvidenceLabel(
        TEXT("SESSION 4 | PLANT / BRACE"),
        TEXT("LEFT PLANT CONTACT GATED  |  DG_BraceAlpha >= 0.85  |  ALL PROFILES"));
    RequestCapture(
        3,
        TEXT("All profiles at the plant/brace phase with the authored left-foot contact gate."),
        TEXT("Plant"),
        TEXT("Three-quarter front composite"),
        EStage::WaitingForRelease);
    if (CaptureEvidence.IsValidIndex(3))
    {
        CaptureEvidence[3].bContactGateRequired = true;
        CaptureEvidence[3].bContactGatePassed = bContactPassed;
    }
    ValidateCompositeFraming(3);
}

void ADiscGolfSession4VisualCaptureRunner::PrepareReleaseCapture()
{
    PauseFixtureMontages(true);
    const bool bReleasePassed = ValidateReleaseGate();
    if (!bReleasePassed)
    {
        Fail(TEXT("release curve, grip, or exactly-once release gate failed"));
        return;
    }
    PositionCompositeCamera(false);
    ShowEvidenceLabel(
        TEXT("SESSION 4 | EXACT RELEASE PHASE"),
        TEXT("3 FIXTURES  |  3 VALID GRIP CALLBACKS  |  0 GAMEPLAY DISCS / 0 STROKES"));
    RequestCapture(
        4,
        TEXT("All profiles at release after exactly one validation-only grip callback each."),
        TEXT("Release"),
        TEXT("Three-quarter front composite"),
        EStage::WaitingForFollowThrough);
    if (CaptureEvidence.IsValidIndex(4))
    {
        CaptureEvidence[4].bReleaseGateRequired = true;
        CaptureEvidence[4].bReleaseGatePassed = bReleasePassed;
    }
    ValidateCompositeFraming(4);
}

void ADiscGolfSession4VisualCaptureRunner::PrepareFollowThroughCapture()
{
    PauseFixtureMontages(true);
    if (!ValidatePhaseGate(TEXT("DG_FollowThroughAlpha"), 0.85f))
    {
        Fail(TEXT("follow-through curve gate was below 0.85"));
        return;
    }
    PositionCompositeCamera(false);
    ShowEvidenceLabel(
        TEXT("SESSION 4 | FOLLOW-THROUGH"),
        TEXT("ALL THREE PROFILES  |  SAME RHBH MONTAGE  |  VALIDATION-ONLY RELEASES"));
    RequestCapture(
        5,
        TEXT("All profiles at the authored follow-through maximum."),
        TEXT("FollowThrough"),
        TEXT("Three-quarter front composite"),
        EStage::WaitingForCreator);
    ValidateCompositeFraming(5);
}

bool ADiscGolfSession4VisualCaptureRunner::PrepareCreatorCapture()
{
    if (!bCreatorOpened)
    {
        DestroyFixtures();
        PossessedGolfer->SetActorHiddenInGame(false);
        PlayerController->SetViewTarget(PossessedGolfer);

        if (!PlayerController->OpenCharacterCreator())
        {
            return false;
        }

        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
            this,
            Widgets,
            UDiscGolfCharacterCreatorWidget::StaticClass(),
            true);
        for (UUserWidget* Widget : Widgets)
        {
            if (UDiscGolfCharacterCreatorWidget* Creator =
                    Cast<UDiscGolfCharacterCreatorWidget>(Widget);
                Creator && Creator->IsInViewport())
            {
                LiveCreatorWidget = Creator;
                break;
            }
        }
        UDiscGolfCharacterProfile* ShortProfile =
            LoadObject<UDiscGolfCharacterProfile>(nullptr, Session4ProfilePaths[0]);
        if (!LiveCreatorWidget || !ShortProfile)
        {
            return false;
        }

        // Left is intentionally shown here: it proves live profile response
        // while making the UI's explicit "animated LHBH unavailable" boundary
        // readable. No montage is requested and Cancel restores the snapshot.
        CreatorEvidenceBody = ShortProfile->Body;
        CreatorEvidenceThrowStyle = ShortProfile->ThrowStyle;
        CreatorEvidenceHandedness = EDGHandedness::Left;
        LiveCreatorWidget->SetDraftProfile(
            CreatorEvidenceBody,
            CreatorEvidenceThrowStyle,
            CreatorEvidenceHandedness);
        bCreatorLiveAdjustmentApplied =
            PlayerController->PreviewCharacterCreatorDraft(
                CreatorEvidenceBody,
                CreatorEvidenceThrowStyle,
                CreatorEvidenceHandedness);
        if (!bCreatorLiveAdjustmentApplied)
        {
            return false;
        }
        bCreatorOpened = true;

        // Use the same possessed pawn and live UI, but frame it with the
        // transient evidence camera. Paused spring-arm state is not a reliable
        // rendered-size contract on every Editor/RHI path.
        USkeletalMeshComponent* PreviewMesh = PossessedGolfer->GetSkeletalGolferMesh();
        const FVector Target = PreviewMesh
            ? PreviewMesh->Bounds.Origin : PossessedGolfer->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
        const FVector Forward = InitialGolferTransform.GetRotation().GetForwardVector().GetSafeNormal();
        const FVector Right = InitialGolferTransform.GetRotation().GetRightVector().GetSafeNormal();
        const FVector CameraLocation = Target + Forward * 465.0f + Right * 65.0f
            + FVector(0.0f, 0.0f, 20.0f);
        CaptureCamera->SetActorLocation(CameraLocation);
        CaptureCamera->SetActorRotation((Target - CameraLocation).Rotation());
        if (UCameraComponent* Camera = CaptureCamera->GetCameraComponent())
        {
            Camera->SetFieldOfView(34.0f);
        }
        PlayerController->SetViewTarget(CaptureCamera);
        if (PlayerController->PlayerCameraManager)
        {
            PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
        }
        return true;
    }

    if (!PlayerController->IsCharacterCreatorOpen()
        || !LiveCreatorWidget
        || !LiveCreatorWidget->IsInViewport())
    {
        return false;
    }

    if (GEngine)
    {
        GEngine->ClearOnScreenDebugMessages();
    }
    PlayerController->SetViewTarget(CaptureCamera);
    PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
    RequestCapture(
        6,
        TEXT("Live native/WBP creator with ShortCompact body and Left handed draft on the possessed pawn; explicit LHBH limitation visible."),
        TEXT("LiveCreatorPreview"),
        TEXT("Possessed-pawn creator camera with live UI"),
        EStage::WaitingForExtremes,
        true);
    ValidateCreatorFraming(6);
    return !bFinished;
}

bool ADiscGolfSession4VisualCaptureRunner::FinishCreatorAndPrepareExtremes()
{
    if (!bCreatorOpened || !PlayerController->IsCharacterCreatorOpen())
    {
        return false;
    }
    PlayerController->CancelCharacterCreator();
    if (PlayerController->IsCharacterCreatorOpen())
    {
        return false;
    }

    FDGBodyProfile RestoredBody;
    FDGThrowStyle RestoredStyle;
    EDGHandedness RestoredHandedness = EDGHandedness::Right;
    bCreatorCancelRestored = PossessedGolfer->GetCharacterCreatorProfile(
            RestoredBody,
            RestoredStyle,
            RestoredHandedness)
        && NearlyEqualBody(RestoredBody, CreatorOpeningBody)
        && NearlyEqualStyle(RestoredStyle, CreatorOpeningThrowStyle)
        && RestoredHandedness == CreatorOpeningHandedness;
    bCreatorOpened = false;
    LiveCreatorWidget = nullptr;
    if (!bCreatorCancelRestored || !ValidateNoGameplayMutation())
    {
        return false;
    }

    PossessedGolfer->SetActorHiddenInGame(true);
    if (!SpawnExtremeFixtures())
    {
        return false;
    }
    ArrangeFixturesFront();
    PlayerController->SetViewTarget(CaptureCamera);
    return true;
}

void ADiscGolfSession4VisualCaptureRunner::PrepareExtremeCapture()
{
    MinimumPhaseCurveValue = -1.0f;
    PositionCompositeCamera(false, true);
    ShowEvidenceLabel(
        TEXT("SESSION 4 | REPRESENTATIVE SLIDER EXTREMES"),
        TEXT("LEFT ALL-MAXIMUM  |  RIGHT ALL-MINIMUM  |  TRANSIENT / NOT SAVED"));
    RequestCapture(
        7,
        TEXT("Schema-minimum and schema-maximum body/throw-style drafts on the same master character foundation."),
        TEXT("NeutralSliderExtremes"),
        TEXT("Front two-profile composite"),
        EStage::Finished);
    ValidateCompositeFraming(7);
}

void ADiscGolfSession4VisualCaptureRunner::PositionCompositeCamera(
    bool bSideView,
    bool bExtremeView)
{
    if (!CaptureCamera || FixtureMeshes.IsEmpty())
    {
        return;
    }
    FBox Composite(ForceInit);
    for (const USkeletalMeshComponent* Mesh : FixtureMeshes)
    {
        if (Mesh)
        {
            Composite += Mesh->Bounds.GetBox();
        }
    }
    const FVector Target = Composite.GetCenter() + FVector(0.0f, 0.0f, 5.0f);
    const FVector Forward = InitialGolferTransform.GetRotation().GetForwardVector().GetSafeNormal();
    const FVector Right = InitialGolferTransform.GetRotation().GetRightVector().GetSafeNormal();
    const FVector Primary = bSideView ? Right : Forward;
    const float Distance = bExtremeView ? 1050.0f : 1325.0f;

    const TArray<FVector> Candidates = {
        Target + Primary * Distance + FVector(0.0f, 0.0f, 55.0f),
        Target - Primary * Distance + FVector(0.0f, 0.0f, 75.0f),
        Target + (Primary + (bSideView ? Forward : Right) * 0.28f).GetSafeNormal()
            * Distance + FVector(0.0f, 0.0f, 105.0f),
        Target - (Primary - (bSideView ? Forward : Right) * 0.28f).GetSafeNormal()
            * Distance + FVector(0.0f, 0.0f, 135.0f)
    };

    FCollisionQueryParams Params(SCENE_QUERY_STAT(Session4VisualCamera), false);
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(CaptureCamera);
    Params.AddIgnoredActor(PossessedGolfer);
    for (ADiscGolferPawn* Fixture : FixturePawns)
    {
        Params.AddIgnoredActor(Fixture);
    }
    FVector Selected = Candidates.Last();
    for (const FVector& Candidate : Candidates)
    {
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(
                Hit, Candidate, Target, ECC_Visibility, Params))
        {
            Selected = Candidate;
            break;
        }
    }
    CaptureCamera->SetActorLocation(Selected);
    CaptureCamera->SetActorRotation((Target - Selected).Rotation());
    if (UCameraComponent* Camera = CaptureCamera->GetCameraComponent())
    {
        Camera->SetFieldOfView(bExtremeView ? 34.0f : 38.0f);
    }
    PlayerController->SetViewTarget(CaptureCamera);
}

bool ADiscGolfSession4VisualCaptureRunner::ValidateCompositeFraming(
    int32 CaptureIndex)
{
    if (!CaptureEvidence.IsValidIndex(CaptureIndex)
        || !CaptureCamera
        || FixtureMeshes.IsEmpty())
    {
        Fail(TEXT("composite framing lacked a capture record, camera, or fixtures"));
        return false;
    }

    FCaptureEvidence& Record = CaptureEvidence[CaptureIndex];
    const FTransform CameraTransform(
        CaptureCamera->GetActorRotation(),
        CaptureCamera->GetActorLocation());
    const float HorizontalTan = FMath::Tan(FMath::DegreesToRadians(
        CaptureCamera->GetCameraComponent()->FieldOfView * 0.5f));
    constexpr float AspectRatio = 16.0f / 9.0f;
    const float VerticalTan = HorizontalTan / AspectRatio;
    const auto Project = [&CameraTransform, HorizontalTan, VerticalTan](const FVector& Point)
    {
        const FVector Local = CameraTransform.InverseTransformPosition(Point);
        if (Local.X <= 1.0f)
        {
            return FVector2D(1000.0f, 1000.0f);
        }
        return FVector2D(
            Local.Y / (Local.X * HorizontalTan),
            Local.Z / (Local.X * VerticalTan));
    };
    const auto IsFramed = [&Project](const FVector& Point, float Margin)
    {
        const FVector2D P = Project(Point);
        return FMath::Abs(P.X) <= Margin && FMath::Abs(P.Y) <= Margin;
    };

    Record.FramedProfileCount = 0;
    Record.MinSubjectScreenHeightFraction = TNumericLimits<float>::Max();
    Record.MaxSubjectScreenHeightFraction = 0.0f;
    Record.bCameraOutsideSubjects = true;
    Record.bFiniteTransforms = true;
    TArray<float> PelvisScreenX;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(Session4VisualFraming), false);
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(CaptureCamera);
    Params.AddIgnoredActor(PossessedGolfer);
    for (ADiscGolferPawn* Fixture : FixturePawns)
    {
        Params.AddIgnoredActor(Fixture);
    }
    Record.bLineOfSightClear = true;

    for (const USkeletalMeshComponent* Mesh : FixtureMeshes)
    {
        const FVector Head = GetBoneLocation(Mesh, TEXT("head"));
        const FVector FootL = GetBoneLocation(Mesh, TEXT("foot_l"));
        const FVector FootR = GetBoneLocation(Mesh, TEXT("foot_r"));
        const FVector HandL = GetBoneLocation(Mesh, TEXT("hand_l"));
        const FVector HandR = GetBoneLocation(Mesh, TEXT("hand_r"));
        const FVector Pelvis = GetBoneLocation(Mesh, TEXT("pelvis"));
        const FVector Feet = (FootL + FootR) * 0.5f;
        const bool bFramed = IsFramed(Head, 0.88f)
            && IsFramed(FootL, 0.90f)
            && IsFramed(FootR, 0.90f)
            && IsFramed(HandL, 0.94f)
            && IsFramed(HandR, 0.94f)
            && IsFramed(Pelvis, 0.88f);
        Record.FramedProfileCount += bFramed ? 1 : 0;
        const float ScreenHeight = FMath::Abs(Project(Head).Y - Project(Feet).Y) * 0.5f;
        Record.MinSubjectScreenHeightFraction = FMath::Min(
            Record.MinSubjectScreenHeightFraction, ScreenHeight);
        Record.MaxSubjectScreenHeightFraction = FMath::Max(
            Record.MaxSubjectScreenHeightFraction, ScreenHeight);
        PelvisScreenX.Add(Project(Pelvis).X);
        Record.bCameraOutsideSubjects &=
            !Mesh->Bounds.GetBox().ExpandBy(35.0f).IsInside(
                CaptureCamera->GetActorLocation());
        Record.bFiniteTransforms &= IsPoseFiniteAndPlausible(Mesh);

        FHitResult Hit;
        Record.bLineOfSightClear &= !GetWorld()->LineTraceSingleByChannel(
            Hit,
            CaptureCamera->GetActorLocation(),
            Mesh->Bounds.Origin,
            ECC_Visibility,
            Params);
    }

    // Reachback and plant must show the real provisional held Cylinder at a
    // fixed physical size. It follows disc_grip_r translation/rotation, but
    // must never inherit profile/Control-Rig socket scale.
    if (CaptureIndex == 2 || CaptureIndex == 3)
    {
        for (int32 FixtureIndex = 0; FixtureIndex < FixturePawns.Num(); ++FixtureIndex)
        {
            const ADiscGolferPawn* Fixture = FixturePawns[FixtureIndex];
            const UStaticMeshComponent* Held = Fixture ? Fixture->GetHeldDiscVisual() : nullptr;
            const USkeletalMeshComponent* Mesh = Fixture ? Fixture->GetSkeletalGolferMesh() : nullptr;
            const FVector Scale = Held ? Held->GetComponentScale() : FVector::ZeroVector;
            const FVector Extent = Held ? Held->Bounds.BoxExtent : FVector::ZeroVector;
            const float GripDistance = Held && Mesh
                ? FVector::Distance(Held->GetComponentLocation(), Mesh->GetSocketLocation(TEXT("disc_grip_r")))
                : TNumericLimits<float>::Max();
            const bool bHeldDiscPassed = Held && Mesh && Held->IsVisible()
                && IsSession4FiniteVector(Held->GetComponentLocation())
                && IsSession4FiniteVector(Scale)
                && Scale.Equals(FVector(0.21f, 0.21f, 0.015f), 0.0025f)
                // World AABB axes change as the hand tilts; no axis may exceed
                // the provisional Cylinder radius, but Z is not its thickness.
                && Extent.X <= 16.0f && Extent.Y <= 16.0f && Extent.Z <= 16.0f
                && Held->Bounds.SphereRadius >= 8.0f && Held->Bounds.SphereRadius <= 16.0f
                && GripDistance <= 2.0f;
            Record.bFiniteTransforms &= bHeldDiscPassed;
            UE_LOG(LogDiscGolfTour, Display,
                TEXT("DG_SESSION4_HELD_DISC_GATE: capture=%d fixture=%d pass=%s visible=%s scale=(%.4f,%.4f,%.4f) extent=(%.2f,%.2f,%.2f) radius=%.2f grip_cm=%.3f"),
                CaptureIndex + 1,
                FixtureIndex,
                bHeldDiscPassed ? TEXT("true") : TEXT("false"),
                Held && Held->IsVisible() ? TEXT("true") : TEXT("false"),
                Scale.X, Scale.Y, Scale.Z,
                Extent.X, Extent.Y, Extent.Z,
                Held ? Held->Bounds.SphereRadius : 0.0f,
                GripDistance);
        }
    }

    if (Record.MinSubjectScreenHeightFraction == TNumericLimits<float>::Max())
    {
        Record.MinSubjectScreenHeightFraction = 0.0f;
    }
    float HorizontalSpread = 0.0f;
    if (PelvisScreenX.Num() >= 2)
    {
        float MinimumX = PelvisScreenX[0];
        float MaximumX = PelvisScreenX[0];
        for (const float ScreenX : PelvisScreenX)
        {
            MinimumX = FMath::Min(MinimumX, ScreenX);
            MaximumX = FMath::Max(MaximumX, ScreenX);
        }
        HorizontalSpread = MaximumX - MinimumX;
    }
    Record.bAllSubjectsFramed =
        Record.FramedProfileCount == FixtureMeshes.Num();
    Record.bReadable = Record.bAllSubjectsFramed
        && Record.MinSubjectScreenHeightFraction >= 0.22f
        && Record.MaxSubjectScreenHeightFraction <= 0.82f
        && HorizontalSpread >= 0.24f
        && Record.bCameraOutsideSubjects
        && Record.bLineOfSightClear
        && Record.bFiniteTransforms;
    Record.WorldDiscDelta = CountWorldDiscs() - BaselineWorldDiscCount;
    Record.StrokeDelta = GameMode->GetStrokes() - BaselineStrokes;

    Record.MinMontageSeconds = TNumericLimits<float>::Max();
    Record.MaxMontageSeconds = -1.0f;
    for (const USkeletalMeshComponent* Mesh : FixtureMeshes)
    {
        const float Position = GetMontagePosition(Mesh);
        if (Position >= 0.0f)
        {
            Record.MinMontageSeconds = FMath::Min(Record.MinMontageSeconds, Position);
            Record.MaxMontageSeconds = FMath::Max(Record.MaxMontageSeconds, Position);
        }
    }
    if (Record.MinMontageSeconds == TNumericLimits<float>::Max())
    {
        Record.MinMontageSeconds = -1.0f;
    }
    Record.MinRequiredCurveValue = MinimumPhaseCurveValue;

    if (!Record.bReadable || Record.WorldDiscDelta != 0 || Record.StrokeDelta != 0)
    {
        Fail(FString::Printf(
            TEXT("capture %d failed empty/cropped/readability or gameplay-mutation gates"),
            CaptureIndex + 1));
        return false;
    }
    return true;
}

bool ADiscGolfSession4VisualCaptureRunner::ValidateCreatorFraming(
    int32 CaptureIndex)
{
    if (!CaptureEvidence.IsValidIndex(CaptureIndex)
        || !LiveCreatorWidget
        || !PlayerController
        || !PlayerController->PlayerCameraManager)
    {
        Fail(TEXT("creator framing lacked its UI, controller, camera, or capture record"));
        return false;
    }
    FCaptureEvidence& Record = CaptureEvidence[CaptureIndex];
    USkeletalMeshComponent* Mesh = PossessedGolfer->GetSkeletalGolferMesh();
    FVector2D HeadScreen;
    FVector2D PelvisScreen;
    FVector2D GripScreen;
    const FVector Head = GetBoneLocation(Mesh, TEXT("head"));
    const FVector Pelvis = GetBoneLocation(Mesh, TEXT("pelvis"));
    const FVector Grip = GetBoneLocation(Mesh, TEXT("disc_grip_l"));
    const bool bHeadProjected = PlayerController->ProjectWorldLocationToScreen(
        Head, HeadScreen, true);
    const bool bPelvisProjected = PlayerController->ProjectWorldLocationToScreen(
        Pelvis, PelvisScreen, true);
    const bool bGripProjected = PlayerController->ProjectWorldLocationToScreen(
        Grip, GripScreen, true);
    const FVector2D WidgetSize = LiveCreatorWidget->GetCachedGeometry().GetLocalSize();

    FDGBodyProfile CurrentBody;
    FDGThrowStyle CurrentStyle;
    EDGHandedness CurrentHandedness = EDGHandedness::Right;
    const bool bCurrentMatchesDraft = PossessedGolfer->GetCharacterCreatorProfile(
            CurrentBody, CurrentStyle, CurrentHandedness)
        && NearlyEqualBody(CurrentBody, CreatorEvidenceBody)
        && NearlyEqualStyle(CurrentStyle, CreatorEvidenceThrowStyle)
        && CurrentHandedness == CreatorEvidenceHandedness;
    const float ScreenHeightFraction = bHeadProjected && bPelvisProjected
        ? FMath::Abs(PelvisScreen.Y - HeadScreen.Y) / static_cast<float>(ExpectedHeight)
        : 0.0f;

    Record.FramedProfileCount = bHeadProjected && bPelvisProjected && bGripProjected ? 1 : 0;
    Record.MinSubjectScreenHeightFraction = ScreenHeightFraction;
    Record.MaxSubjectScreenHeightFraction = ScreenHeightFraction;
    Record.bAllSubjectsFramed = bHeadProjected && bPelvisProjected && bGripProjected
        && HeadScreen.X >= 700.0f && HeadScreen.X <= 1900.0f
        && PelvisScreen.X >= 700.0f && PelvisScreen.X <= 1900.0f
        && GripScreen.X >= 700.0f && GripScreen.X <= 1900.0f
        && HeadScreen.Y >= 15.0f && PelvisScreen.Y <= 1065.0f
        && GripScreen.Y >= 15.0f && GripScreen.Y <= 1065.0f;
    Record.bCameraOutsideSubjects =
        !Mesh->Bounds.GetBox().ExpandBy(35.0f).IsInside(
            PlayerController->PlayerCameraManager->GetCameraLocation());
    FCollisionQueryParams Params(SCENE_QUERY_STAT(Session4CreatorFraming), false);
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(PossessedGolfer);
    Params.AddIgnoredActor(CaptureCamera);
    FHitResult Hit;
    Record.bLineOfSightClear = !GetWorld()->LineTraceSingleByChannel(
        Hit,
        PlayerController->PlayerCameraManager->GetCameraLocation(),
        Mesh->Bounds.Origin,
        ECC_Visibility,
        Params);
    Record.bFiniteTransforms = IsPoseFiniteAndPlausible(Mesh);
    Record.bReadable = Record.bAllSubjectsFramed
        && ScreenHeightFraction >= 0.16f
        && ScreenHeightFraction <= 0.70f
        && WidgetSize.X >= 1200.0f
        && WidgetSize.Y >= 700.0f
        && LiveCreatorWidget->IsInViewport()
        && PlayerController->IsCharacterCreatorOpen()
        && bCurrentMatchesDraft
        && CreatorEvidenceHandedness == EDGHandedness::Left
        && Record.bCameraOutsideSubjects
        && Record.bLineOfSightClear
        && Record.bFiniteTransforms;
    Record.MinMontageSeconds = -1.0f;
    Record.MaxMontageSeconds = -1.0f;
    Record.MinRequiredCurveValue = -1.0f;
    Record.WorldDiscDelta = CountWorldDiscs() - BaselineWorldDiscCount;
    Record.StrokeDelta = GameMode->GetStrokes() - BaselineStrokes;
    if (!Record.bReadable || Record.WorldDiscDelta != 0 || Record.StrokeDelta != 0)
    {
        Fail(TEXT("live creator capture was cropped, unreadable, stale, or changed gameplay"));
        return false;
    }
    return true;
}

bool ADiscGolfSession4VisualCaptureRunner::IsPoseFiniteAndPlausible(
    const USkeletalMeshComponent* Mesh) const
{
    if (!Mesh || !Mesh->GetSkeletalMeshAsset()
        || !Mesh->GetComponentTransform().IsValid()
        || !IsSession4FiniteVector(Mesh->Bounds.Origin)
        || !IsSession4FiniteVector(Mesh->Bounds.BoxExtent))
    {
        return false;
    }
    static const FName RequiredBones[] = {
        TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
        TEXT("spine_04"), TEXT("neck_01"), TEXT("head"),
        TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
        TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
        TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"),
        TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r"),
        TEXT("disc_grip_l"), TEXT("disc_grip_r")
    };
    const FVector Pelvis = GetBoneLocation(Mesh, TEXT("pelvis"));
    for (const FName Bone : RequiredBones)
    {
        const FVector Location = GetBoneLocation(Mesh, Bone);
        if (!IsSession4FiniteVector(Location) || FVector::Dist(Location, Pelvis) > 400.0f)
        {
            return false;
        }
    }
    const FBodyMetrics Metrics = MeasureBody(Mesh);
    return Metrics.bFiniteAndPlausible
        && Mesh->Bounds.SphereRadius >= 40.0f
        && Mesh->Bounds.SphereRadius <= 450.0f;
}

ADiscGolfSession4VisualCaptureRunner::FBodyMetrics
ADiscGolfSession4VisualCaptureRunner::MeasureBody(
    const USkeletalMeshComponent* Mesh) const
{
    FBodyMetrics Metrics;
    if (!Mesh)
    {
        return Metrics;
    }
    const FVector Head = GetBoneLocation(Mesh, TEXT("head"));
    const FVector FootL = GetBoneLocation(Mesh, TEXT("foot_l"));
    const FVector FootR = GetBoneLocation(Mesh, TEXT("foot_r"));
    const FVector BallL = GetBoneLocation(Mesh, TEXT("ball_l"));
    const FVector BallR = GetBoneLocation(Mesh, TEXT("ball_r"));
    const float FeetZ = FMath::Min(
        FMath::Min(FootL.Z, FootR.Z),
        FMath::Min(BallL.Z, BallR.Z));
    Metrics.HeightLandmarkCm = FMath::Abs(Head.Z - FeetZ);
    Metrics.BoundsHeightCm = Mesh->Bounds.BoxExtent.Z * 2.0f;
    Metrics.ShoulderWidthCm = BoneDistance(Mesh, TEXT("upperarm_l"), TEXT("upperarm_r"));

    const auto ArmLength = [this, Mesh](const TCHAR* Side)
    {
        const FName Upper(*FString::Printf(TEXT("upperarm_%s"), Side));
        const FName Lower(*FString::Printf(TEXT("lowerarm_%s"), Side));
        const FName Hand(*FString::Printf(TEXT("hand_%s"), Side));
        const FName Middle1(*FString::Printf(TEXT("middle_01_%s"), Side));
        const FName Middle2(*FString::Printf(TEXT("middle_02_%s"), Side));
        const FName Middle3(*FString::Printf(TEXT("middle_03_%s"), Side));
        return BoneDistance(Mesh, Upper, Lower)
            + BoneDistance(Mesh, Lower, Hand)
            + BoneDistance(Mesh, Hand, Middle1)
            + BoneDistance(Mesh, Middle1, Middle2)
            + BoneDistance(Mesh, Middle2, Middle3);
    };
    const float LeftArm = ArmLength(TEXT("l"));
    const float RightArm = ArmLength(TEXT("r"));
    Metrics.WingspanChainCm = Metrics.ShoulderWidthCm + LeftArm + RightArm;

    const auto HandLength = [this, Mesh](const TCHAR* Side)
    {
        const FName Hand(*FString::Printf(TEXT("hand_%s"), Side));
        const FName Middle1(*FString::Printf(TEXT("middle_01_%s"), Side));
        const FName Middle2(*FString::Printf(TEXT("middle_02_%s"), Side));
        const FName Middle3(*FString::Printf(TEXT("middle_03_%s"), Side));
        return BoneDistance(Mesh, Hand, Middle1)
            + BoneDistance(Mesh, Middle1, Middle2)
            + BoneDistance(Mesh, Middle2, Middle3);
    };
    Metrics.HandLengthCm = (HandLength(TEXT("l")) + HandLength(TEXT("r"))) * 0.5f;
    Metrics.FootLengthCm = (BoneDistance(Mesh, TEXT("foot_l"), TEXT("ball_l"))
        + BoneDistance(Mesh, TEXT("foot_r"), TEXT("ball_r"))) * 0.5f;
    Metrics.GripOffsetCm = (BoneDistance(Mesh, TEXT("hand_l"), TEXT("disc_grip_l"))
        + BoneDistance(Mesh, TEXT("hand_r"), TEXT("disc_grip_r"))) * 0.5f;

    Metrics.bFiniteAndPlausible =
        FMath::IsFinite(Metrics.HeightLandmarkCm)
        && FMath::IsFinite(Metrics.BoundsHeightCm)
        && FMath::IsFinite(Metrics.WingspanChainCm)
        && FMath::IsFinite(Metrics.ShoulderWidthCm)
        && FMath::IsFinite(Metrics.HandLengthCm)
        && FMath::IsFinite(Metrics.FootLengthCm)
        && FMath::IsFinite(Metrics.GripOffsetCm)
        && Metrics.HeightLandmarkCm >= 90.0f && Metrics.HeightLandmarkCm <= 260.0f
        && Metrics.BoundsHeightCm >= 100.0f && Metrics.BoundsHeightCm <= 360.0f
        && Metrics.WingspanChainCm >= 90.0f && Metrics.WingspanChainCm <= 300.0f
        && Metrics.ShoulderWidthCm >= 15.0f && Metrics.ShoulderWidthCm <= 100.0f
        && Metrics.HandLengthCm >= 3.0f && Metrics.HandLengthCm <= 40.0f
        && Metrics.FootLengthCm >= 5.0f && Metrics.FootLengthCm <= 50.0f
        && Metrics.GripOffsetCm >= 1.0f && Metrics.GripOffsetCm <= 40.0f;
    return Metrics;
}

bool ADiscGolfSession4VisualCaptureRunner::RecordNeutralMetrics()
{
    if (FixtureMeshes.Num() != ActiveProfileEvidenceIndices.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < FixtureMeshes.Num(); ++Index)
    {
        if (!ProfileEvidence.IsValidIndex(ActiveProfileEvidenceIndices[Index]))
        {
            return false;
        }
        USkeletalMeshComponent* Mesh = FixtureMeshes[Index];
        Mesh->RefreshBoneTransforms();
        Mesh->UpdateBounds();
        FProfileEvidence& Evidence =
            ProfileEvidence[ActiveProfileEvidenceIndices[Index]];
        Evidence.NeutralMetrics = MeasureBody(Mesh);
        Evidence.NeutralLeftFootZ = GetBoneLocation(Mesh, TEXT("foot_l")).Z;
        Evidence.NeutralRightFootZ = GetBoneLocation(Mesh, TEXT("foot_r")).Z;
        Evidence.bFiniteNeutralPose = IsPoseFiniteAndPlausible(Mesh);
        if (!Evidence.NeutralMetrics.bFiniteAndPlausible
            || !Evidence.bFiniteNeutralPose)
        {
            return false;
        }
    }
    return true;
}

bool ADiscGolfSession4VisualCaptureRunner::ValidateProfileOrdering() const
{
    if (ProfileEvidence.Num() < 5)
    {
        return false;
    }
    const FBodyMetrics& Short = ProfileEvidence[0].NeutralMetrics;
    const FBodyMetrics& Baseline = ProfileEvidence[1].NeutralMetrics;
    const FBodyMetrics& Tall = ProfileEvidence[2].NeutralMetrics;
    const FBodyMetrics& Minimum = ProfileEvidence[3].NeutralMetrics;
    const FBodyMetrics& Maximum = ProfileEvidence[4].NeutralMetrics;
    return Short.HeightLandmarkCm < Baseline.HeightLandmarkCm
        && Baseline.HeightLandmarkCm < Tall.HeightLandmarkCm
        && Short.WingspanChainCm < Tall.WingspanChainCm
        && Minimum.HeightLandmarkCm < Maximum.HeightLandmarkCm
        && Minimum.WingspanChainCm < Maximum.WingspanChainCm
        && Minimum.ShoulderWidthCm < Maximum.ShoulderWidthCm
        && Minimum.HandLengthCm < Maximum.HandLengthCm;
}

FVector ADiscGolfSession4VisualCaptureRunner::GetBoneLocation(
    const USkeletalMeshComponent* Mesh,
    FName Bone) const
{
    return Mesh ? Mesh->GetBoneLocation(Bone, EBoneSpaces::WorldSpace)
        : FVector::ZeroVector;
}

float ADiscGolfSession4VisualCaptureRunner::BoneDistance(
    const USkeletalMeshComponent* Mesh,
    FName A,
    FName B) const
{
    return FVector::Dist(GetBoneLocation(Mesh, A), GetBoneLocation(Mesh, B));
}

void ADiscGolfSession4VisualCaptureRunner::RequestCapture(
    int32 CaptureIndex,
    const FString& Evidence,
    const FString& Phase,
    const FString& CameraView,
    EStage NextStage,
    bool bUseCaptureCamera)
{
    if (bScreenshotPending
        || CaptureIndex < 0
        || CaptureIndex >= UE_ARRAY_COUNT(Session4CaptureFilenames)
        || CaptureEvidence.Num() != CaptureIndex)
    {
        Fail(TEXT("capture request was invalid, out of order, or overlapped another screenshot"));
        return;
    }

    FCaptureEvidence Record;
    Record.Filename = Session4CaptureFilenames[CaptureIndex];
    Record.Evidence = Evidence;
    Record.Phase = Phase;
    Record.CameraView = CameraView;
    CaptureEvidence.Add(Record);

    PendingCaptureIndex = CaptureIndex;
    PendingCapturePath = FPaths::Combine(OutputDirectory, Record.Filename);
    IFileManager::Get().Delete(*PendingCapturePath, false, true, true);
    bScreenshotPending = true;
    bScreenshotIssued = false;
    bPendingUsesCaptureCamera = bUseCaptureCamera;
    StageAfterCapture = NextStage;
    SetStage(EStage::Capturing);
    if (bUseCaptureCamera)
    {
        PlayerController->SetViewTarget(CaptureCamera);
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION4_VISUAL_CAPTURE: FRAMED %d/8 %s"),
        CaptureIndex + 1,
        *PendingCapturePath);
}

bool ADiscGolfSession4VisualCaptureRunner::PollPendingCapture()
{
    if (!bScreenshotIssued)
    {
        bScreenshotIssued = true;
        StageStartRealSeconds = FPlatformTime::Seconds();
        FScreenshotRequest::RequestScreenshot(
            PendingCapturePath,
            true,
            false,
            false,
            FIntRect(),
            true);
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION4_VISUAL_CAPTURE: REQUEST %d/8 %s"),
            PendingCaptureIndex + 1,
            *PendingCapturePath);
        return false;
    }

    const int64 FileSize = IFileManager::Get().FileSize(*PendingCapturePath);
    if (FileSize >= MinimumReadablePngBytes)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *PendingCapturePath)
            || Bytes.Num() < 24
            || Bytes[0] != 0x89 || Bytes[1] != 0x50
            || Bytes[2] != 0x4E || Bytes[3] != 0x47)
        {
            Fail(FString::Printf(
                TEXT("capture was not a readable PNG: %s"),
                *PendingCapturePath));
            return false;
        }

        FCaptureEvidence& Record = CaptureEvidence[PendingCaptureIndex];
        Record.Bytes = FileSize;
        Record.Width = ReadSession4BigEndianInt32(&Bytes[16]);
        Record.Height = ReadSession4BigEndianInt32(&Bytes[20]);
        Record.Sha1 = ComputeSha1(Bytes);
        if (Record.Width != ExpectedWidth || Record.Height != ExpectedHeight
            || Record.Sha1.Len() != FSHAHash::GetStringLen()
            || !Record.bReadable)
        {
            Fail(FString::Printf(
                TEXT("capture %d failed exact 1920x1080, hash, or readability gates (%dx%d, bytes=%lld)"),
                PendingCaptureIndex + 1,
                Record.Width,
                Record.Height,
                Record.Bytes));
            return false;
        }

        const int32 CompletedIndex = PendingCaptureIndex;
        bScreenshotPending = false;
        bScreenshotIssued = false;
        PendingCaptureIndex = INDEX_NONE;
        PendingCapturePath.Reset();
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION4_VISUAL_CAPTURE: CAPTURED %d/8 1920x1080 bytes=%lld sha1=%s"),
            CompletedIndex + 1,
            Record.Bytes,
            *Record.Sha1);
        HandleCaptureCompleted(CompletedIndex);
        return true;
    }

    if (SecondsInStage() > Session4ScreenshotTimeoutSeconds)
    {
        Fail(FString::Printf(TEXT("screenshot timed out: %s"), *PendingCapturePath));
    }
    return false;
}

void ADiscGolfSession4VisualCaptureRunner::HandleCaptureCompleted(int32 CaptureIndex)
{
    switch (CaptureIndex)
    {
        case 0:
            ArrangeFixturesSide();
            SetStage(EStage::WaitingForNeutralSide);
            break;

        case 1:
            ArrangeFixturesFront();
            if (!BeginFixtureThrows())
            {
                Fail(TEXT("three validation-only profile montages did not start"));
                return;
            }
            SetStage(EStage::WaitingForReachback);
            break;

        case 2:
            PauseFixtureMontages(false);
            SetStage(EStage::WaitingForPlant);
            break;

        case 3:
            PauseFixtureMontages(false);
            SetStage(EStage::WaitingForRelease);
            break;

        case 4:
            PauseFixtureMontages(false);
            SetStage(EStage::WaitingForFollowThrough);
            break;

        case 5:
            SetStage(EStage::WaitingForCreator);
            break;

        case 6:
            if (!FinishCreatorAndPrepareExtremes())
            {
                Fail(TEXT("creator Cancel did not restore the opening profile or extremes could not spawn"));
                return;
            }
            SetStage(EStage::WaitingForExtremes);
            break;

        case 7:
            Pass();
            break;

        default:
            Fail(TEXT("unknown Session 4 capture completion index"));
            break;
    }
}

void ADiscGolfSession4VisualCaptureRunner::ShowEvidenceLabel(
    const FString& Heading,
    const FString& Detail) const
{
    if (!GEngine)
    {
        return;
    }
    GEngine->ClearOnScreenDebugMessages();
    GEngine->AddOnScreenDebugMessage(
        41004, 30.0f, FColor(198, 235, 255), Detail, true, FVector2D(1.05f));
    GEngine->AddOnScreenDebugMessage(
        41003, 30.0f, FColor::White, Heading, true, FVector2D(1.45f));
}

FString ADiscGolfSession4VisualCaptureRunner::ComputeSha1(
    const TArray<uint8>& Bytes) const
{
    return Bytes.IsEmpty()
        ? FString()
        : FSHA1::HashBuffer(Bytes.GetData(), static_cast<uint64>(Bytes.Num())).ToString();
}

void ADiscGolfSession4VisualCaptureRunner::SnapshotPersistentFiles(
    const FString& RootDirectory,
    bool bPackagesOnly,
    TMap<FString, FFileStamp>& OutFiles) const
{
    OutFiles.Reset();
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*RootDirectory))
    {
        return;
    }
    PlatformFile.IterateDirectoryRecursively(
        *RootDirectory,
        [bPackagesOnly, &OutFiles](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
        {
            if (bIsDirectory)
            {
                return true;
            }
            FString Path = FPaths::ConvertRelativePathToFull(FilenameOrDirectory);
            FPaths::MakeStandardFilename(Path);
            const FString Extension = FPaths::GetExtension(Path, true).ToLower();
            if (bPackagesOnly
                && Extension != TEXT(".uasset")
                && Extension != TEXT(".umap"))
            {
                return true;
            }
            FFileStamp Stamp;
            Stamp.Size = IFileManager::Get().FileSize(*Path);
            Stamp.TimestampTicks = IFileManager::Get().GetTimeStamp(*Path).GetTicks();
            OutFiles.Add(Path, Stamp);
            return true;
        });
}

void ADiscGolfSession4VisualCaptureRunner::DiffPersistentFiles(
    const TMap<FString, FFileStamp>& Before,
    const TMap<FString, FFileStamp>& After,
    TArray<FString>& OutChanged) const
{
    OutChanged.Reset();
    TSet<FString> Paths;
    for (const TPair<FString, FFileStamp>& Pair : Before)
    {
        Paths.Add(Pair.Key);
    }
    for (const TPair<FString, FFileStamp>& Pair : After)
    {
        Paths.Add(Pair.Key);
    }
    for (const FString& Path : Paths)
    {
        const FFileStamp* OldStamp = Before.Find(Path);
        const FFileStamp* NewStamp = After.Find(Path);
        if (!OldStamp || !NewStamp || !(*OldStamp == *NewStamp))
        {
            OutChanged.Add(Path);
        }
    }
    OutChanged.Sort();
}

bool ADiscGolfSession4VisualCaptureRunner::VerifyNoPersistentWrites()
{
    TMap<FString, FFileStamp> FinalPackages;
    TMap<FString, FFileStamp> FinalSaveGames;
    SnapshotPersistentFiles(FPaths::ProjectContentDir(), true, FinalPackages);
    SnapshotPersistentFiles(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")),
        false,
        FinalSaveGames);
    DiffPersistentFiles(InitialPackageFiles, FinalPackages, ChangedPackageFiles);
    DiffPersistentFiles(InitialSaveGameFiles, FinalSaveGames, ChangedSaveGameFiles);
    bPersistentWriteCheckPassed = ChangedPackageFiles.IsEmpty()
        && ChangedSaveGameFiles.IsEmpty();
    return bPersistentWriteCheckPassed;
}

int32 ADiscGolfSession4VisualCaptureRunner::CountWorldDiscs() const
{
    int32 Count = 0;
    if (GetWorld())
    {
        for (TActorIterator<ADiscActor> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It) && !It->IsActorBeingDestroyed())
            {
                ++Count;
            }
        }
    }
    return Count;
}

void ADiscGolfSession4VisualCaptureRunner::ApplyEvidenceMaterial(
    USkeletalMeshComponent* Mesh,
    const FLinearColor& Color) const
{
    if (!Mesh)
    {
        return;
    }
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BaseMaterial)
    {
        UMaterialInstanceDynamic* Dynamic =
            UMaterialInstanceDynamic::Create(BaseMaterial, Mesh);
        if (Dynamic)
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
            Dynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
            // The generated validation proxy has no authored material slots,
            // but its render sections still address slot zero. An explicit
            // transient override makes the evidence legend truthful without
            // modifying SK_DG_Master.
            Mesh->SetMaterial(0, Dynamic);
        }
    }
    Mesh->MarkRenderStateDirty();
}

void ADiscGolfSession4VisualCaptureRunner::RestoreRuntimeState()
{
    if (PlayerController && PlayerController->IsCharacterCreatorOpen())
    {
        PlayerController->CancelCharacterCreator();
    }
    DestroyFixtures();
    if (PossessedGolfer)
    {
        PossessedGolfer->SetActorHiddenInGame(bInitialGolferHidden);
    }
    if (PlayerController)
    {
        if (AHUD* Hud = PlayerController->GetHUD())
        {
            Hud->bShowHUD = bInitialHudVisible;
        }
        if (InitialViewTarget.IsValid())
        {
            PlayerController->SetViewTarget(InitialViewTarget.Get());
        }
        else if (PossessedGolfer)
        {
            PlayerController->SetViewTarget(PossessedGolfer);
        }
    }
    if (GEngine)
    {
        GEngine->ClearOnScreenDebugMessages();
    }
    if (CaptureCamera && IsValid(CaptureCamera))
    {
        CaptureCamera->Destroy();
    }
    CaptureCamera = nullptr;
    CaptureKeyLight = nullptr;
    CaptureFillLight = nullptr;
    LiveCreatorWidget = nullptr;
}

void ADiscGolfSession4VisualCaptureRunner::WriteManifest(
    bool bPassed,
    const FString& Error)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    Root->SetStringField(
        TEXT("scope"),
        TEXT("SESSION_4_CHARACTER_CREATOR_MANUAL_VISUAL_EVIDENCE_ONLY"));
    Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("output_directory"), OutputDirectory);
    Root->SetStringField(TEXT("error"), Error);
    Root->SetNumberField(TEXT("expected_capture_count"), 8);
    Root->SetNumberField(TEXT("capture_count"), CaptureEvidence.Num());
    Root->SetNumberField(TEXT("expected_width"), ExpectedWidth);
    Root->SetNumberField(TEXT("expected_height"), ExpectedHeight);

    TSharedRef<FJsonObject> Assets = MakeShared<FJsonObject>();
    for (const FRequiredAsset& Required : RequiredAssetPaths)
    {
        Assets->SetStringField(Required.Label, Required.Path);
    }
    Assets->SetStringField(
        TEXT("single_foundation_assertion"),
        TEXT("Every transient fixture reports the exact SK_DG_Master, SKEL_DG_Master, and ABP_DG_Player paths; IK_DG_Master and CR_DG_Master are required preflight assets."));
    Root->SetObjectField(TEXT("assets"), Assets);

    TSharedRef<FJsonObject> PhaseTargets = MakeShared<FJsonObject>();
    PhaseTargets->SetNumberField(TEXT("reachback_seconds"), ReachbackSeconds);
    PhaseTargets->SetStringField(TEXT("reachback_curve"), TEXT("DG_ReachbackAlpha"));
    PhaseTargets->SetNumberField(TEXT("plant_seconds"), PlantSeconds);
    PhaseTargets->SetStringField(TEXT("plant_curve"), TEXT("DG_BraceAlpha + DG_FootPlant_L"));
    PhaseTargets->SetNumberField(TEXT("release_seconds"), ReleaseSeconds);
    PhaseTargets->SetStringField(TEXT("release_curve"), TEXT("DG_ReleaseApproachAlpha"));
    PhaseTargets->SetNumberField(TEXT("follow_through_seconds"), FollowThroughSeconds);
    PhaseTargets->SetStringField(TEXT("follow_through_curve"), TEXT("DG_FollowThroughAlpha"));
    Root->SetObjectField(TEXT("phase_targets"), PhaseTargets);

    TArray<TSharedPtr<FJsonValue>> Profiles;
    for (const FProfileEvidence& Profile : ProfileEvidence)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("name"), Profile.Name);
        Object->SetStringField(TEXT("asset"), Profile.AssetPath);
        Object->SetStringField(TEXT("fixture_kind"), Profile.FixtureKind);
        Object->SetStringField(
            TEXT("handedness"),
            Profile.Handedness == EDGHandedness::Left ? TEXT("Left") : TEXT("Right"));
        Object->SetObjectField(TEXT("body_profile"), BodyToJson(Profile.Body));
        Object->SetObjectField(TEXT("throw_style"), StyleToJson(Profile.ThrowStyle));

        TSharedRef<FJsonObject> Metrics = MakeShared<FJsonObject>();
        Metrics->SetNumberField(
            TEXT("height_head_to_ground_landmark_cm"),
            Profile.NeutralMetrics.HeightLandmarkCm);
        Metrics->SetNumberField(
            TEXT("skeletal_bounds_height_cm"),
            Profile.NeutralMetrics.BoundsHeightCm);
        Metrics->SetNumberField(
            TEXT("wingspan_chain_cm"),
            Profile.NeutralMetrics.WingspanChainCm);
        Metrics->SetNumberField(
            TEXT("shoulder_width_cm"),
            Profile.NeutralMetrics.ShoulderWidthCm);
        Metrics->SetNumberField(
            TEXT("average_hand_chain_cm"),
            Profile.NeutralMetrics.HandLengthCm);
        Metrics->SetNumberField(
            TEXT("average_foot_to_ball_cm"),
            Profile.NeutralMetrics.FootLengthCm);
        Metrics->SetNumberField(
            TEXT("average_hand_to_grip_cm"),
            Profile.NeutralMetrics.GripOffsetCm);
        Metrics->SetBoolField(
            TEXT("finite_and_plausible"),
            Profile.NeutralMetrics.bFiniteAndPlausible);
        Object->SetObjectField(TEXT("measured_metrics"), Metrics);

        Object->SetNumberField(TEXT("release_commit_count"), Profile.ReleaseCommitCount);
        Object->SetBoolField(TEXT("same_skeletal_mesh"), Profile.bSameSkeletalMesh);
        Object->SetBoolField(TEXT("same_skeleton"), Profile.bSameSkeleton);
        Object->SetBoolField(TEXT("same_animation_blueprint"), Profile.bSameAnimBlueprint);
        Object->SetBoolField(TEXT("finite_neutral_pose"), Profile.bFiniteNeutralPose);
        Profiles.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("profiles"), Profiles);

    TArray<TSharedPtr<FJsonValue>> Captures;
    for (const FCaptureEvidence& Capture : CaptureEvidence)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("filename"), Capture.Filename);
        Object->SetStringField(
            TEXT("path"),
            FPaths::Combine(OutputDirectory, Capture.Filename));
        Object->SetStringField(TEXT("evidence"), Capture.Evidence);
        Object->SetStringField(TEXT("phase"), Capture.Phase);
        Object->SetStringField(TEXT("camera_view"), Capture.CameraView);
        Object->SetStringField(TEXT("hash_algorithm"), TEXT("SHA-1"));
        Object->SetStringField(TEXT("sha1"), Capture.Sha1);
        Object->SetNumberField(TEXT("bytes"), static_cast<double>(Capture.Bytes));
        Object->SetNumberField(TEXT("width"), Capture.Width);
        Object->SetNumberField(TEXT("height"), Capture.Height);
        Object->SetNumberField(
            TEXT("framed_profile_count"),
            Capture.FramedProfileCount);
        Object->SetNumberField(
            TEXT("minimum_subject_screen_height_fraction"),
            Capture.MinSubjectScreenHeightFraction);
        Object->SetNumberField(
            TEXT("maximum_subject_screen_height_fraction"),
            Capture.MaxSubjectScreenHeightFraction);
        Object->SetNumberField(
            TEXT("minimum_montage_seconds"),
            Capture.MinMontageSeconds);
        Object->SetNumberField(
            TEXT("maximum_montage_seconds"),
            Capture.MaxMontageSeconds);
        Object->SetNumberField(
            TEXT("minimum_required_curve_value"),
            Capture.MinRequiredCurveValue);
        Object->SetBoolField(TEXT("all_subjects_framed"), Capture.bAllSubjectsFramed);
        Object->SetBoolField(TEXT("camera_outside_subjects"), Capture.bCameraOutsideSubjects);
        Object->SetBoolField(TEXT("line_of_sight_clear"), Capture.bLineOfSightClear);
        Object->SetBoolField(TEXT("finite_transforms"), Capture.bFiniteTransforms);
        Object->SetBoolField(TEXT("readability_gate"), Capture.bReadable);
        Object->SetBoolField(
            TEXT("contact_gate_required"),
            Capture.bContactGateRequired);
        Object->SetBoolField(
            TEXT("contact_gate_passed"),
            Capture.bContactGatePassed);
        Object->SetBoolField(
            TEXT("release_gate_required"),
            Capture.bReleaseGateRequired);
        Object->SetBoolField(
            TEXT("release_gate_passed"),
            Capture.bReleaseGatePassed);
        Object->SetNumberField(TEXT("world_disc_delta"), Capture.WorldDiscDelta);
        Object->SetNumberField(TEXT("stroke_delta"), Capture.StrokeDelta);
        Captures.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("captures"), Captures);

    TSharedRef<FJsonObject> Handedness = MakeShared<FJsonObject>();
    Handedness->SetBoolField(TEXT("passed"), bHandednessBoundaryPassed);
    Handedness->SetStringField(TEXT("fixture"), TEXT("Baseline transient pawn"));
    Handedness->SetStringField(TEXT("previewed_handedness"), TEXT("Left"));
    Handedness->SetBoolField(
        TEXT("animated_rhbh_rejected_before_montage"),
        bHandednessRejectedBeforeMontage);
    Handedness->SetBoolField(
        TEXT("release_count_zero_during_boundary_check"),
        bHandednessRejectedBeforeMontage);
    Handedness->SetBoolField(
        TEXT("profile_primary_asset_unchanged"),
        bHandednessProfileAssetUnchanged);
    Handedness->SetStringField(
        TEXT("limitation"),
        TEXT("Animated LHBH is not authored in Session 4. Left-handed gameplay retains the existing non-animated fallback; no mirrored montage is claimed."));
    Root->SetObjectField(TEXT("handedness_boundary"), Handedness);

    TSharedRef<FJsonObject> Creator = MakeShared<FJsonObject>();
    Creator->SetBoolField(TEXT("opened_live_ui"), bCreatorLiveAdjustmentApplied);
    Creator->SetStringField(
        TEXT("widget"),
        TEXT("WBP_DG_CharacterCreator subclass when available; otherwise native UDiscGolfCharacterCreatorWidget"));
    Creator->SetStringField(TEXT("preview_subject"), TEXT("possessed ADiscGolferPawn"));
    Creator->SetObjectField(TEXT("live_body_profile"), BodyToJson(CreatorEvidenceBody));
    Creator->SetObjectField(TEXT("live_throw_style"), StyleToJson(CreatorEvidenceThrowStyle));
    Creator->SetStringField(TEXT("live_handedness"), TEXT("Left"));
    Creator->SetBoolField(
        TEXT("explicit_lhbh_limitation_visible"),
        bCreatorLiveAdjustmentApplied
            && CreatorEvidenceHandedness == EDGHandedness::Left);
    Creator->SetBoolField(TEXT("cancel_restored_opening_profile"), bCreatorCancelRestored);
    Creator->SetBoolField(TEXT("saved"), false);
    Root->SetObjectField(TEXT("live_creator"), Creator);

    TSharedRef<FJsonObject> GameplayIsolation = MakeShared<FJsonObject>();
    GameplayIsolation->SetNumberField(
        TEXT("baseline_world_disc_count"),
        BaselineWorldDiscCount);
    GameplayIsolation->SetNumberField(
        TEXT("final_world_disc_count"),
        CountWorldDiscs());
    GameplayIsolation->SetNumberField(TEXT("baseline_strokes"), BaselineStrokes);
    GameplayIsolation->SetNumberField(
        TEXT("final_strokes"),
        GameMode ? GameMode->GetStrokes() : -1);
    GameplayIsolation->SetNumberField(
        TEXT("validation_only_fixture_launch_callbacks"),
        FixtureLaunchCount);
    GameplayIsolation->SetStringField(
        TEXT("fixture_launch_mode"),
        TEXT("BOUND VALIDATION CALLBACK; NO GAMEPLAY DISC OR STROKE"));
    Root->SetObjectField(TEXT("gameplay_isolation"), GameplayIsolation);

    const auto StringsToJson = [](const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Json;
        for (const FString& Value : Values)
        {
            Json.Add(MakeShared<FJsonValueString>(Value));
        }
        return Json;
    };
    TSharedRef<FJsonObject> Writes = MakeShared<FJsonObject>();
    Writes->SetBoolField(TEXT("passed"), bPersistentWriteCheckPassed);
    Writes->SetNumberField(
        TEXT("initial_package_file_count"),
        InitialPackageFiles.Num());
    Writes->SetNumberField(
        TEXT("initial_save_game_file_count"),
        InitialSaveGameFiles.Num());
    Writes->SetArrayField(
        TEXT("changed_uasset_or_umap_files"),
        StringsToJson(ChangedPackageFiles));
    Writes->SetArrayField(
        TEXT("changed_save_game_files"),
        StringsToJson(ChangedSaveGameFiles));
    Writes->SetStringField(
        TEXT("allowed_outputs"),
        TEXT("Exactly eight PNG files and this JSON manifest under Saved/CharacterFramework/Screenshots/Session4_CharacterCreator."));
    Writes->SetBoolField(TEXT("package_save_calls"), false);
    Writes->SetBoolField(TEXT("level_save_calls"), false);
    Root->SetObjectField(TEXT("persistent_write_guard"), Writes);

    TSharedRef<FJsonObject> Limitations = MakeShared<FJsonObject>();
    Limitations->SetStringField(
        TEXT("character_art"),
        TEXT("Validation proxy only; final production character topology, skinning, face, hair, clothing, and deformation polish remain pending."));
    Limitations->SetStringField(
        TEXT("held_disc"),
        TEXT("Provisional Cylinder only; production rim/palm ergonomics are not accepted by this pass."));
    Limitations->SetStringField(
        TEXT("left_handed_animation"),
        TEXT("LHBH montage is intentionally unavailable; the existing non-animated gameplay fallback remains authoritative."));
    Limitations->SetStringField(
        TEXT("environment"),
        TEXT("Final Fab forest visual acceptance remains pending imported marketplace assets and is outside this evidence run."));
    Root->SetObjectField(TEXT("honest_limitations"), Limitations);

    FString Json;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    FFileHelper::SaveStringToFile(Json, *ManifestPath);
}

void ADiscGolfSession4VisualCaptureRunner::Fail(const FString& Reason)
{
    if (bFinished)
    {
        return;
    }
    bFinished = true;
    FailureReason = Reason;
    SetStage(EStage::Finished);
    VerifyNoPersistentWrites();
    RestoreRuntimeState();
    WriteManifest(false, Reason);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("DG_SESSION4_VISUAL_CAPTURE: FAIL %s | captures=%d manifest=%s"),
        *Reason,
        CaptureEvidence.Num(),
        *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession4VisualCaptureRunner::Pass()
{
    if (bFinished)
    {
        return;
    }

    bool bCaptureFilesPassed = CaptureEvidence.Num() == 8;
    for (const FCaptureEvidence& Capture : CaptureEvidence)
    {
        bCaptureFilesPassed &= Capture.Width == ExpectedWidth
            && Capture.Height == ExpectedHeight
            && Capture.Bytes >= MinimumReadablePngBytes
            && Capture.Sha1.Len() == FSHAHash::GetStringLen()
            && Capture.bReadable
            && Capture.bFiniteTransforms
            && Capture.WorldDiscDelta == 0
            && Capture.StrokeDelta == 0
            && (!Capture.bContactGateRequired || Capture.bContactGatePassed)
            && (!Capture.bReleaseGateRequired || Capture.bReleaseGatePassed);
    }
    const bool bRegularReleaseCountsPassed = ProfileEvidence.Num() >= 3
        && ProfileEvidence[0].ReleaseCommitCount == 1
        && ProfileEvidence[1].ReleaseCommitCount == 1
        && ProfileEvidence[2].ReleaseCommitCount == 1
        && FixtureLaunchCount == 3
        && FixtureLaunchGripTransforms.Num() == 3;
    const bool bProfilesPassed = ValidateProfileOrdering();
    const bool bNoGameplayMutation = ValidateNoGameplayMutation();
    const bool bNoWrites = VerifyNoPersistentWrites();

    if (!bCaptureFilesPassed
        || !bRegularReleaseCountsPassed
        || !bProfilesPassed
        || !bNoGameplayMutation
        || !bNoWrites
        || !bCreatorLiveAdjustmentApplied
        || !bCreatorCancelRestored
        || !bHandednessBoundaryPassed)
    {
        Fail(TEXT("final Session 4 visual evidence invariant was incomplete"));
        return;
    }

    bFinished = true;
    SetStage(EStage::Finished);
    RestoreRuntimeState();
    WriteManifest(true, TEXT(""));
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION4_VISUAL_CAPTURE: PASS captures=8 profiles=5 fixture_releases=3 manifest=%s"),
        *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}
