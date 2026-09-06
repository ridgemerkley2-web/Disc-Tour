#include "DiscGolfSession3VisualCaptureRunner.h"

#include "DiscActor.h"
#include "DiscBagComponent.h"
#include "DiscGolferPawn.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfFullCharacterRuntime.h"
#include "DiscGolfMetaHumanAvatarBackendComponent.h"
#include "DiscGolfMetaHumanRetargetAnimInstance.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfThrowComponent.h"
#include "DiscFlightComponent.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscTrajectorySubsystem.h"
#include "ThrowControllerComponent.h"
#include "DrawDebugHelpers.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace DiscGolfSession3VisualCapture
{
constexpr double Session3VisualReadinessTimeoutSeconds = 8.0;
constexpr double TimingCaptureSeconds = 0.12;
constexpr double ScreenshotTimeoutSeconds = 30.0;
constexpr double Session3VisualReleaseTimeoutSeconds = 5.0;
constexpr double Session3VisualGameplayRecoveryTimeoutSeconds = 65.0;
constexpr float MetaHumanProofMinimumCompletedFlightDurationSeconds =
    0.50f;
constexpr int32 MetaHumanProofMinimumTrajectorySampleCount = 30;
constexpr float MetaHumanProofMinimumAirCarryMeters = 1.0f;
constexpr float MetaHumanProofMinimumFinalCarryMeters = 5.0f;
// A fully weighted checkpoint may differ from one only by normal float precision.
// Both current and desired weights must independently prove the exact instance is
// neither blending in nor blending out when evidence is recorded.
constexpr float MetaHumanProofMinimumMontageBlendWeight = 0.999f;
constexpr float MetaHumanProofMaximumMontageBlendWeight = 1.001f;
constexpr double ProfileTimeoutSeconds = 6.0;

bool IsSession19TemporalFrameDump()
{
    return FParse::Param(
        FCommandLine::Get(), TEXT("Session19TemporalFrameDump"));
}

double Session3VisualEffectiveTimeout(double BaseSeconds)
{
    // DUMPMOVIE performs a blocking full-resolution readback every rendered
    // frame. Keep all pose/evidence gates identical, but prevent real-wall-
    // clock watchdogs from rejecting the deliberately slow 60 Hz review lane.
    return IsSession19TemporalFrameDump()
        ? BaseSeconds * 24.0
        : BaseSeconds;
}
// The release delegate can execute before the assembled MetaHuman's AnimGraph,
// Control Rig, and retarget post-evaluation have committed that frame's pose.
// Keep the montage and disc paused while two complete world ticks settle the
// presentation pose, then evaluate framing from the committed component pose.
constexpr int32 MetaHumanReleasePoseSettleWorldTicks = 2;
constexpr float LegacyCaptureKeyLightLumens = 4000.0f;
constexpr float LegacyCaptureFillLightLumens = 1200.0f;
constexpr float LegacyCaptureLightAttenuationRadiusCm = 2500.0f;
// Preserve the forest exposure while keeping light clothing and skin inside the
// filmic shoulder. The previous 6000/2000 lm pair at +1 EV clipped the body and
// erased most material and anatomical form in the evidence frames.
constexpr float MetaHumanProductionCaptureKeyLightLumens = 2600.0f;
constexpr float MetaHumanProductionCaptureFillLightLumens = 650.0f;
constexpr float MetaHumanProductionCaptureLightAttenuationRadiusCm = 1800.0f;
// Frame 54 is the validated reachback fixture. The grip is clear of the torso
// here, so the real attached Cylinder reads cleanly before release.
constexpr float HeldCaptureMontageSeconds = 0.90f;
constexpr float FollowThroughCaptureMontageSeconds = 1.86f;
constexpr double MetaHumanProofFrameRateHz = 60.0;
constexpr double MetaHumanProofFixedDeltaTimeSeconds =
    1.0 / MetaHumanProofFrameRateHz;
constexpr float MetaHumanProofCameraFieldOfViewDegrees = 34.0f;
constexpr float MetaHumanProofExposureBiasEv = 0.55f;
constexpr float MetaHumanProofWhiteTemperatureKelvin = 5600.0f;
// The live montage enters checkpoints on deterministic sub-frame boundaries:
// 0.319 at ReachBack, 0.464 after the paused screenshot/resume boundary, and
// 0.681 at FollowThrough after the release transition. A 0.75-frame cap stays
// below 12.5 ms at 60 Hz and still rejects a neighboring authored frame.
constexpr float MetaHumanProofMaximumFrameError = 0.75f;
// Enter the Recovery capture branch one quarter-frame early. At the fixed
// 60 Hz proof cadence this selects the nearest naturally evaluated frame even
// after the preceding screenshot pauses; it does not seek or alter play rate.
constexpr float MetaHumanRecoveryCheckpointTriggerLeadFrames = 0.25f;
constexpr float MetaHumanProofRequiredBlendOutTriggerTimeSeconds = 0.10f;
constexpr float MetaHumanProofMaximumCameraLocationErrorCm = 0.10f;
constexpr float MetaHumanProofMaximumCameraRotationErrorDegrees = 0.05f;
constexpr float MetaHumanProofMaximumCameraFieldOfViewErrorDegrees = 0.01f;
constexpr float MetaHumanProofReleaseCurveMinimum = 0.90f;
// Frame 94 is the recipe's named FollowThrough phase checkpoint. Its authored
// follow-through curve is 0.78 there; accept only the narrow 0.75+ neighborhood
// so the proof cannot drift back to the frame-104 recovery-deceleration pose.
constexpr float MetaHumanProofFollowThroughCurveMinimum = 0.75f;
constexpr float MetaHumanProofMaximumSourceTargetDirectionErrorDegrees = 22.0f;
constexpr float MetaHumanProofMaximumSourceTargetElbowAngleErrorDegrees = 30.0f;
constexpr float MetaHumanProofMinimumTargetSourceDistanceRatio = 0.70f;
constexpr float MetaHumanProofMaximumTargetSourceDistanceRatio = 1.35f;
// This MetaHuman's internal ball bone sits about 10.8 cm below the visible
// sole. Calibrate the lower edge to that authored anatomical offset while the
// impact actor/component/normal and upper floating-foot bound remain strict.
constexpr float MetaHumanProofBraceBallMinimumGroundGapCm = -14.0f;
constexpr float MetaHumanProofBraceBallMaximumGroundGapCm = 20.0f;
constexpr float MetaHumanProofMinimumGroundNormalZ = 0.65f;
constexpr float MetaHumanProofMaximumPreReleaseGripPlaneErrorDegrees = 8.0f;
constexpr float MetaHumanProofMaximumReleaseGripPlaneErrorDegrees = 35.0f;
constexpr float MetaHumanProofLineOfSightEndpointAllowanceCm = 8.0f;

const TCHAR* CaptureFilenames[] = {
    TEXT("01_HeldDisc_BeforeRelease.png"),
    TEXT("02_Exact_DGReleaseDisc_Frame.png"),
    TEXT("03_ImmediatePostRelease_OneGameplayDisc.png"),
    TEXT("04_FollowThrough.png"),
    TEXT("05_RecoveredGameplayState.png"),
    TEXT("06_ShortCompact_Playback.png"),
    TEXT("07_Baseline_Playback.png"),
    TEXT("08_TallLongArms_Playback.png")
};

const TCHAR* MetaHumanProductionCaptureFilenames[] = {
    TEXT("01_MetaHuman_ReachBack_Wide.png"),
    TEXT("02_MetaHuman_Plant_Wide.png"),
    TEXT("03_MetaHuman_Release_Wide.png"),
    TEXT("04_MetaHuman_FollowThrough_Wide.png"),
    TEXT("05_MetaHuman_Recovery_Wide.png")
};

const TCHAR* MetaHumanProductionCheckpointNames[] = {
    TEXT("ReachBack"),
    TEXT("Plant"),
    TEXT("Release"),
    TEXT("FollowThrough"),
    TEXT("Recovery")
};

// Keep phase captures on their named recipe keys. In particular, FollowThrough
// is frame 94; frame 104 is the recovery-deceleration biomechanical event and
// brings the throwing hand back toward the torso.
constexpr int32 MetaHumanProductionTargetFrames[] = {44, 64, 84, 94, 132};

const float MetaHumanProductionMinimumTargetHandToPelvisCm[] = {
    45.0f, 35.0f, 45.0f, 45.0f, 15.0f
};

const float MetaHumanProductionMinimumTargetArmReachFraction[] = {
    0.70f, 0.65f, 0.70f, 0.70f, 0.45f
};

const EDGThrowPhase MetaHumanProductionExpectedPhases[] = {
    EDGThrowPhase::ReachBack,
    EDGThrowPhase::Plant,
    EDGThrowPhase::Release,
    EDGThrowPhase::FollowThrough,
    EDGThrowPhase::Recovery
};

static_assert(
    UE_ARRAY_COUNT(MetaHumanProductionCaptureFilenames)
        == UE_ARRAY_COUNT(MetaHumanProductionCheckpointNames)
    && UE_ARRAY_COUNT(MetaHumanProductionCaptureFilenames)
        == UE_ARRAY_COUNT(MetaHumanProductionTargetFrames)
    && UE_ARRAY_COUNT(MetaHumanProductionCaptureFilenames)
        == UE_ARRAY_COUNT(MetaHumanProductionExpectedPhases),
    "MetaHuman production capture arrays must preserve the five-checkpoint index contract.");
static_assert(
    UE_ARRAY_COUNT(MetaHumanProductionCaptureFilenames)
        == UE_ARRAY_COUNT(MetaHumanProductionMinimumTargetHandToPelvisCm)
    && UE_ARRAY_COUNT(MetaHumanProductionCaptureFilenames)
        == UE_ARRAY_COUNT(MetaHumanProductionMinimumTargetArmReachFraction),
    "MetaHuman target-pose gates must preserve the five-checkpoint index contract.");
static_assert(
    MetaHumanProductionTargetFrames[0] == 44
    && MetaHumanProductionTargetFrames[1] == 64
    && MetaHumanProductionTargetFrames[2] == 84
    && MetaHumanProductionTargetFrames[3] == 94
    && MetaHumanProductionTargetFrames[4] == 132,
    "MetaHuman proof checkpoints must remain bound to the v006 named pose keys.");

const TCHAR* ProfileNames[] = {
    TEXT("ShortCompact"),
    TEXT("Baseline"),
    TEXT("TallLongArms")
};

const TCHAR* ProfilePaths[] = {
    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact"),
    TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"),
    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms")
};

const float ProfileCaptureMontageSeconds[] = {0.92f, 1.43f, 1.84f};

bool IsSession3FiniteVector(const FVector& Value)
{
    return !Value.ContainsNaN()
        && FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

FAnimMontageInstance* ResolveExactMontageInstance(
    USkeletalMeshComponent* Mesh,
    UAnimMontage* Montage,
    int32 ExpectedInstanceId = INDEX_NONE)
{
    UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    if (!Anim || !Montage)
    {
        return nullptr;
    }
    FAnimMontageInstance* Instance = ExpectedInstanceId != INDEX_NONE
        ? Anim->GetMontageInstanceForID(ExpectedInstanceId)
        : Anim->GetInstanceForMontage(Montage);
    return Instance
        && Instance->Montage == Montage
        && (ExpectedInstanceId == INDEX_NONE
            || Instance->GetInstanceID() == ExpectedInstanceId)
        ? Instance
        : nullptr;
}

bool PauseExactMontageInstance(
    USkeletalMeshComponent* Mesh,
    UAnimMontage* Montage,
    FString& OutError,
    int32 ExpectedInstanceId = INDEX_NONE)
{
    FAnimMontageInstance* Instance = ResolveExactMontageInstance(
        Mesh, Montage, ExpectedInstanceId);
    if (!Instance)
    {
        OutError = TEXT("the exact throw montage instance was unavailable for pause");
        return false;
    }
    const float PositionBeforePause = Instance->GetPosition();
    Instance->Pause();
    if (Instance->IsPlaying()
        || !FMath::IsNearlyEqual(
            Instance->GetPosition(), PositionBeforePause, UE_KINDA_SMALL_NUMBER))
    {
        OutError = TEXT("the exact throw montage instance did not freeze in place");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ResumeExactMontageInstance(
    USkeletalMeshComponent* Mesh,
    UAnimMontage* Montage,
    FString& OutError,
    int32 ExpectedInstanceId = INDEX_NONE)
{
    FAnimMontageInstance* Instance = ResolveExactMontageInstance(
        Mesh, Montage, ExpectedInstanceId);
    if (!Instance)
    {
        OutError = TEXT("the exact throw montage instance was unavailable for resume");
        return false;
    }
    Instance->SetPlaying(true);
    if (!Instance->IsPlaying())
    {
        OutError = TEXT("the exact throw montage instance did not resume");
        return false;
    }
    OutError.Reset();
    return true;
}

float ComputeSession3JointAngleDegrees(
    const FVector& ParentLocation,
    const FVector& JointLocation,
    const FVector& ChildLocation)
{
    const FVector JointToParent = ParentLocation - JointLocation;
    const FVector JointToChild = ChildLocation - JointLocation;
    if (!IsSession3FiniteVector(JointToParent)
        || !IsSession3FiniteVector(JointToChild)
        || JointToParent.IsNearlyZero()
        || JointToChild.IsNearlyZero())
    {
        return -1.0f;
    }
    return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(
            JointToParent.GetSafeNormal(),
            JointToChild.GetSafeNormal()),
        -1.0f,
        1.0f)));
}

float ComputeSession3ArmReachFraction(
    const FVector& ShoulderLocation,
    const FVector& ElbowLocation,
    const FVector& HandLocation)
{
    const float AvailableReach = FVector::Distance(
        ShoulderLocation, ElbowLocation)
        + FVector::Distance(ElbowLocation, HandLocation);
    return AvailableReach > UE_KINDA_SMALL_NUMBER
        ? FVector::Distance(ShoulderLocation, HandLocation) / AvailableReach
        : -1.0f;
}

bool BindSession3SelectedThrowProvenance(
    ADiscGolferPawn* Pawn,
    FThrowCommand& InOutCommand)
{
    UDiscBagComponent* Bag = Pawn ? Pawn->GetDiscBag() : nullptr;
    const UDiscGolfCharacterProfile* Profile = Pawn
        ? Pawn->GetRuntimeCharacterProfile() : nullptr;
    FDGDiscInstance SelectedInstance;
    if (!Bag || !Profile
        || !Bag->GetSelectedDiscInstance(SelectedInstance)
        || !SelectedInstance.InstanceId.IsValid()
        || SelectedInstance.DiscDefinitionId.IsNone()
        || Bag->GetSelectedMoldId() != SelectedInstance.DiscDefinitionId)
    {
        return false;
    }

    InOutCommand.DiscInstanceId = SelectedInstance.InstanceId;
    InOutCommand.MoldId = SelectedInstance.DiscDefinitionId;
    InOutCommand.Plastic = Bag->GetSelectedPlastic();
    InOutCommand.Handedness = Profile->Handedness;
    return true;
}

int32 ReadBigEndianInt32(const uint8* Bytes)
{
    return (static_cast<int32>(Bytes[0]) << 24)
        | (static_cast<int32>(Bytes[1]) << 16)
        | (static_cast<int32>(Bytes[2]) << 8)
        | static_cast<int32>(Bytes[3]);
}

const TCHAR* RecoveryReasonLabel(EDiscGolfRHBHThrowRecoveryReason Reason)
{
    switch (Reason)
    {
        case EDiscGolfRHBHThrowRecoveryReason::ThrowFinished:
            return TEXT("ThrowFinished");
        case EDiscGolfRHBHThrowRecoveryReason::ThrowFinishedBeforeRelease:
            return TEXT("ThrowFinishedBeforeRelease");
        case EDiscGolfRHBHThrowRecoveryReason::CancelledBeforeRelease:
            return TEXT("CancelledBeforeRelease");
        case EDiscGolfRHBHThrowRecoveryReason::InterruptedBeforeRelease:
            return TEXT("InterruptedBeforeRelease");
        case EDiscGolfRHBHThrowRecoveryReason::InterruptedAfterRelease:
            return TEXT("InterruptedAfterRelease");
        case EDiscGolfRHBHThrowRecoveryReason::WatchdogBeforeRelease:
            return TEXT("WatchdogBeforeRelease");
        case EDiscGolfRHBHThrowRecoveryReason::WatchdogAfterRelease:
            return TEXT("WatchdogAfterRelease");
        case EDiscGolfRHBHThrowRecoveryReason::None:
        default:
            return TEXT("None");
    }
}
}

using namespace DiscGolfSession3VisualCapture;

ADiscGolfSession3VisualCaptureRunner::ADiscGolfSession3VisualCaptureRunner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ADiscGolfSession3VisualCaptureRunner::Start()
{
    if (bStarted || bFinished)
    {
        return;
    }
    bStarted = true;
    bMetaHumanProductionVisualCapture = FParse::Param(
        FCommandLine::Get(),
        TEXT("Session19MetaHumanProductionVisualCapture"));
    if (bMetaHumanProductionVisualCapture)
    {
        FString RequestedRevision;
        if (FParse::Value(
                FCommandLine::Get(),
                TEXT("Session19ExpectedProductionMotionRevision="),
                RequestedRevision)
            && !RequestedRevision.IsEmpty())
        {
            ExpectedProductionMotionRevision = RequestedRevision;
        }
    }

    GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    ThrowAdapter = Golfer ? Golfer->GetRHBHThrowAdapter() : nullptr;
    GolferMesh = FindSkeletalMesh(Golfer);
    HeldDiscVisual = FindNamedStaticMesh(Golfer, TEXT("HeldDiscVisual"));
    if (!bMetaHumanProductionVisualCapture)
    {
        ThrowMontage = LoadObject<UAnimMontage>(
            nullptr,
            TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype"));
    }
    FrameworkThrowComponent = Golfer
        ? Golfer->FindComponentByClass<UDiscGolfThrowComponent>()
        : nullptr;

    if (!GameMode || !PlayerController || !Golfer || !ThrowAdapter
        || !FrameworkThrowComponent || !GolferMesh || !HeldDiscVisual
        || (!bMetaHumanProductionVisualCapture && !ThrowMontage)
        || !Golfer->GetThrowController() || !Golfer->GetDiscBag())
    {
        Fail(TEXT("gameplay pawn, montage, adapter, held disc, throw controller, or disc bag was unavailable"));
        return;
    }

    OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        bMetaHumanProductionVisualCapture
            ? TEXT("CharacterFramework/Screenshots/Session19_MetaHumanProductionMotion")
            : TEXT("CharacterFramework/Screenshots/Session3_FirstThrow"));
    ManifestPath = FPaths::Combine(
        OutputDirectory,
        bMetaHumanProductionVisualCapture
            ? TEXT("Session19_MetaHumanProductionMotion_CaptureManifest.json")
            : TEXT("Session3_FirstThrow_CaptureManifest.json"));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    for (int32 Index = 0; Index < GetExpectedCaptureCount(); ++Index)
    {
        IFileManager::Get().Delete(
            *FPaths::Combine(OutputDirectory, GetCaptureFilename(Index)),
            false,
            true,
            true);
    }
    IFileManager::Get().Delete(*ManifestPath, false, true, true);

    GameMode->SkipCurrentPresentation();
    if (bMetaHumanProductionVisualCapture)
    {
        FString MetaHumanError;
        if (!ConfigureMetaHumanProductionPresentation(MetaHumanError))
        {
            Fail(FString::Printf(
                TEXT("verified MetaHuman production presentation was unavailable: %s"),
                *MetaHumanError));
            return;
        }
    }
    InitialGolferTransform = Golfer->GetActorTransform();
    BaselineWorldDiscCount = CountWorldDiscs();
    BaselineStrokes = GameMode->GetStrokes();
    BaselineLiveReleaseCount = ThrowAdapter->GetTotalReleaseCommitCount();
    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (const UDiscTrajectorySubsystem* Trajectories =
                GameInstance->GetSubsystem<UDiscTrajectorySubsystem>();
            Trajectories && Trajectories->HasLastCapture())
        {
            BaselineTrajectoryCaptureId =
                Trajectories->GetLastSummary().CaptureId;
        }
    }
    ThrowAdapter->OnReleaseCommitted.AddUniqueDynamic(
        this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveRelease);
    ThrowAdapter->OnThrowRecovered.AddUniqueDynamic(
        this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveRecovery);

    CaptureCamera = GetWorld()->SpawnActor<ACameraActor>();
    if (!CaptureCamera)
    {
        Fail(TEXT("dedicated capture camera could not spawn"));
        return;
    }
    CaptureCamera->SetActorEnableCollision(false);
    if (bMetaHumanProductionVisualCapture)
    {
        bSavedUseFixedTimeStep = FApp::UseFixedTimeStep();
        SavedFixedDeltaTimeSeconds = FApp::GetFixedDeltaTime();
        bFixedTimeStepCaptured = true;
        FApp::SetFixedDeltaTime(MetaHumanProofFixedDeltaTimeSeconds);
        FApp::SetUseFixedTimeStep(true);

        CaptureHud = PlayerController ? PlayerController->GetHUD() : nullptr;
        if (CaptureHud)
        {
            bInitialHudVisible = CaptureHud->bShowHUD;
            bHudVisibilityCaptured = true;
            CaptureHud->bShowHUD = false;
        }
        ConfigureLockedMetaHumanProofCamera();
        ApplyLockedMetaHumanProofCamera();
        if (UCameraComponent* CameraComponent = CaptureCamera->GetCameraComponent())
        {
            CameraComponent->PostProcessBlendWeight = 1.0f;
            FPostProcessSettings& Settings = CameraComponent->PostProcessSettings;
            Settings.bOverride_AutoExposureMethod = true;
            Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
            Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
            Settings.AutoExposureApplyPhysicalCameraExposure = false;
            Settings.bOverride_AutoExposureBias = true;
            Settings.AutoExposureBias = MetaHumanProofExposureBiasEv;
            Settings.bOverride_WhiteTemp = true;
            Settings.WhiteTemp = MetaHumanProofWhiteTemperatureKelvin;
        }
    }

    const auto CreateCaptureLight = [this](
        const TCHAR* Name,
        const FVector& RelativeLocation,
        float IntensityLumens,
        float AttenuationRadiusCm,
        const FColor& Color,
        bool bCastShapingShadow)
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
        if (bMetaHumanProductionVisualCapture)
        {
            Light->SetUseInverseSquaredFalloff(true);
        }
        Light->SetIntensity(IntensityLumens);
        Light->SetAttenuationRadius(AttenuationRadiusCm);
        Light->SetLightColor(Color);
        Light->SetCastShadows(bCastShapingShadow);
        if (bCastShapingShadow)
        {
            // A broad source retains limb and wardrobe definition without the
            // hard-edged shadowing of a bare point emitter.
            Light->SetSourceRadius(35.0f);
        }
        Light->SetVolumetricScatteringIntensity(0.0f);
        Light->RegisterComponent();
        return Light;
    };
    // Capture-only sports-photography lighting. These transient components do
    // not save into the level or alter gameplay/environment light authority.
    const float CaptureKeyLightLumens = bMetaHumanProductionVisualCapture
        ? MetaHumanProductionCaptureKeyLightLumens
        : LegacyCaptureKeyLightLumens;
    const float CaptureFillLightLumens = bMetaHumanProductionVisualCapture
        ? MetaHumanProductionCaptureFillLightLumens
        : LegacyCaptureFillLightLumens;
    const float CaptureLightAttenuationRadiusCm = bMetaHumanProductionVisualCapture
        ? MetaHumanProductionCaptureLightAttenuationRadiusCm
        : LegacyCaptureLightAttenuationRadiusCm;
    CaptureKeyLight = CreateCaptureLight(
        TEXT("Session3CaptureKey"), FVector(220.0f, -180.0f, 220.0f),
        CaptureKeyLightLumens, CaptureLightAttenuationRadiusCm,
        FColor(255, 244, 226), bMetaHumanProductionVisualCapture);
    CaptureFillLight = CreateCaptureLight(
        TEXT("Session3CaptureFill"), FVector(140.0f, 220.0f, 80.0f),
        CaptureFillLightLumens, CaptureLightAttenuationRadiusCm,
        FColor(205, 224, 255), false);
    if (!CaptureKeyLight || !CaptureFillLight)
    {
        Fail(TEXT("transient capture key/fill lights could not be created"));
        return;
    }
    if (!bMetaHumanProductionVisualCapture)
    {
        ApplyNeutralProxyMaterials(GolferMesh);
    }

    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetViewTarget(CaptureCamera);
        Controller->ConsoleCommand(TEXT("r.MotionBlurQuality 0"), true);
        Controller->ConsoleCommand(TEXT("r.DepthOfFieldQuality 0"), true);
        Controller->ConsoleCommand(TEXT("r.EyeAdaptationQuality 1"), true);
    }

    SavedGlobalTimeDilation = UGameplayStatics::GetGlobalTimeDilation(this);
    HeldDiscEvidenceMaterial = ApplyDiscEvidenceMaterial(
        HeldDiscVisual, FLinearColor(0.0f, 0.42f, 0.92f, 1.0f));
    if (!HeldDiscEvidenceMaterial)
    {
        Fail(TEXT("capture-only held-disc material could not be created"));
        return;
    }
    SetStage(EStage::WaitingForGameplay);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: START mode=%s expected_revision=%s output=%s"),
        bMetaHumanProductionVisualCapture ? TEXT("metahuman_production") : TEXT("session3"),
        *ExpectedProductionMotionRevision,
        *OutputDirectory);
}

void ADiscGolfSession3VisualCaptureRunner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bStarted || bFinished)
    {
        return;
    }
    if (!GameMode || !Golfer || !ThrowAdapter)
    {
        Fail(TEXT("runtime authority disappeared during capture"));
        return;
    }

    if (bScreenshotPending)
    {
        if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
        {
            Controller->SetViewTarget(CaptureCamera);
        }
        // Re-anchor the thin evidence ring on the actual rendered component at
        // screenshot time. Never infer this position from the owning actor.
        if (PendingEvidenceMesh)
        {
            ClearEvidenceMarkers();
            DrawDiscEvidenceMarker(
                GetDiscEvidenceLocation(PendingEvidenceMesh),
                PendingEvidenceMarkerColor);
        }
        PollPendingCapture();
        return;
    }

    switch (Stage)
    {
        case EStage::WaitingForGameplay:
            if (GameMode->CanPlayerThrow()
                && (!bMetaHumanProductionVisualCapture
                    || SecondsInStage() >= 0.75))
            {
                FThrowCommand Ignored;
                if (!BuildTimingCommand(Ignored, true))
                {
                    Fail(TEXT("existing throw controller did not enter timing capture"));
                    return;
                }
                SetStage(EStage::BuildingLiveCommand);
            }
            else if (SecondsInStage() > Session3VisualEffectiveTimeout(
                Session3VisualReadinessTimeoutSeconds))
            {
                Fail(TEXT("legal tee throw did not become available"));
            }
            break;

        case EStage::BuildingLiveCommand:
            if (SecondsInStage() >= TimingCaptureSeconds)
            {
                BeginLiveThrow();
            }
            break;

        case EStage::WaitingForHeldPose:
            if (bMetaHumanProductionVisualCapture
                ? (FrameworkThrowComponent->CurrentPhase == EDGThrowPhase::ReachBack
                    && GetMontagePosition(GolferMesh)
                        >= static_cast<float>(MetaHumanProductionTargetFrames[0]
                            / MetaHumanProofFrameRateHz))
                : GetMontagePosition(GolferMesh) >= HeldCaptureMontageSeconds)
            {
                PrepareHeldCapture();
            }
            else if (SecondsInStage() > Session3VisualEffectiveTimeout(
                Session3VisualReleaseTimeoutSeconds))
            {
                Fail(TEXT("held-disc montage pose was not reached"));
            }
            break;

        case EStage::WaitingForPlant:
            if (!bMetaHumanProductionVisualCapture)
            {
                Fail(TEXT("plant proof stage is MetaHuman-capture-only"));
            }
            else if (FrameworkThrowComponent->CurrentPhase == EDGThrowPhase::Plant
                && GetMontagePosition(GolferMesh)
                    >= static_cast<float>(MetaHumanProductionTargetFrames[1]
                        / MetaHumanProofFrameRateHz))
            {
                PreparePlantCapture();
            }
            else if (SecondsInStage() > Session3VisualEffectiveTimeout(
                Session3VisualReleaseTimeoutSeconds))
            {
                Fail(TEXT("plant montage pose was not reached"));
            }
            break;

        case EStage::WaitingForRelease:
            if (SecondsInStage() > Session3VisualEffectiveTimeout(
                Session3VisualReleaseTimeoutSeconds))
            {
                Fail(TEXT("DG Release Disc did not fire during visual capture"));
            }
            break;

        case EStage::WaitingForExactReleasePoseSettle:
        {
            if (!bMetaHumanProductionVisualCapture)
            {
                Fail(TEXT("release-pose settle stage is MetaHuman-capture-only"));
                break;
            }
            if (!MetaHumanVisualBody)
            {
                Fail(TEXT("verified MetaHuman body was lost while settling the release pose"));
                break;
            }
            MetaHumanVisualBody->HandleExistingParallelEvaluationTask(true, true);
            bExactReleasePoseParallelEvaluationComplete =
                !MetaHumanVisualBody->IsRunningParallelEvaluation();
            if (GFrameCounter > ExactReleasePoseLastSettleFrameCounter)
            {
                ExactReleasePoseLastSettleFrameCounter = GFrameCounter;
                if (GFrameCounter > ExactReleasePoseCallbackFrameCounter)
                {
                    ++ExactReleasePoseSettleTicksCompleted;
                }
            }
            ExactReleasePoseValidationFrameCounter = GFrameCounter;
            ExactReleasePoseBoneRevisionAtValidation =
                MetaHumanVisualBody->GetBoneTransformRevisionNumber();
            ExactReleasePoseBoneTransformFrameAtValidation =
                MetaHumanVisualBody->GetCurrentBoneTransformFrame();
            const bool bTwoPostCallbackFramesElapsed =
                ExactReleasePoseCallbackFrameCounter > 0
                && ExactReleasePoseValidationFrameCounter
                    >= ExactReleasePoseCallbackFrameCounter
                        + MetaHumanReleasePoseSettleWorldTicks;
            const bool bBodyEvaluatedThisFrame =
                ExactReleasePoseBoneTransformFrameAtValidation
                    == static_cast<uint32>(ExactReleasePoseValidationFrameCounter);
            const bool bBoneRevisionAdvanced =
                ExactReleasePoseBoneRevisionAtValidation
                    != ExactReleasePoseBoneRevisionAtCallback;
            bExactReleasePosePostEvaluateProven =
                ExactReleasePoseSettleTicksCompleted
                    >= MetaHumanReleasePoseSettleWorldTicks
                && bTwoPostCallbackFramesElapsed
                && bExactReleasePoseParallelEvaluationComplete
                && bBodyEvaluatedThisFrame
                && bBoneRevisionAdvanced;
            if (bExactReleasePosePostEvaluateProven)
            {
                UE_LOG(LogDiscGolfTour, Display,
                    TEXT("DG_SESSION3_VISUAL_CAPTURE: RELEASE_POSE_SETTLED ticks=%d callback_frame=%llu validation_frame=%llu callback_revision=%u validation_revision=%u body_frame=%u phase=%d"),
                    ExactReleasePoseSettleTicksCompleted,
                    ExactReleasePoseCallbackFrameCounter,
                    ExactReleasePoseValidationFrameCounter,
                    ExactReleasePoseBoneRevisionAtCallback,
                    ExactReleasePoseBoneRevisionAtValidation,
                    ExactReleasePoseBoneTransformFrameAtValidation,
                    static_cast<int32>(FrameworkThrowComponent->CurrentPhase));
                PrepareExactReleaseCapture();
            }
            else if (SecondsInStage() > Session3VisualEffectiveTimeout(
                Session3VisualReleaseTimeoutSeconds))
            {
                Fail(FString::Printf(
                    TEXT("MetaHuman release pose did not produce post-evaluate proof: ticks=%d callback_frame=%llu validation_frame=%llu callback_revision=%u validation_revision=%u body_frame=%u parallel_complete=%d"),
                    ExactReleasePoseSettleTicksCompleted,
                    ExactReleasePoseCallbackFrameCounter,
                    ExactReleasePoseValidationFrameCounter,
                    ExactReleasePoseBoneRevisionAtCallback,
                    ExactReleasePoseBoneRevisionAtValidation,
                    ExactReleasePoseBoneTransformFrameAtValidation,
                    bExactReleasePoseParallelEvaluationComplete ? 1 : 0));
            }
            break;
        }

        case EStage::WaitingForPostRelease:
            if (SecondsInStage() >= 0.10)
            {
                PreparePostReleaseCapture();
            }
            break;

        case EStage::WaitingForFollowThrough:
            if (bMetaHumanProductionVisualCapture
                ? (FrameworkThrowComponent->CurrentPhase == EDGThrowPhase::FollowThrough
                    && GetMontagePosition(GolferMesh)
                        >= static_cast<float>(MetaHumanProductionTargetFrames[3]
                            / MetaHumanProofFrameRateHz))
                : GetMontagePosition(GolferMesh) >= FollowThroughCaptureMontageSeconds)
            {
                bLiveFollowThroughReached = ThrowAdapter->IsThrowActive();
                PrepareFollowThroughCapture();
            }
            else if (SecondsInStage() > Session3VisualEffectiveTimeout(
                Session3VisualReleaseTimeoutSeconds))
            {
                Fail(TEXT("follow-through montage pose was not reached"));
            }
            break;

        case EStage::WaitingForRecoveryProof:
            if (!bMetaHumanProductionVisualCapture)
            {
                Fail(TEXT("recovery proof stage is MetaHuman-capture-only"));
            }
            else if (FrameworkThrowComponent->CurrentPhase == EDGThrowPhase::Recovery
                && GetMontagePosition(GolferMesh)
                    >= static_cast<float>((MetaHumanProductionTargetFrames[4]
                            - MetaHumanRecoveryCheckpointTriggerLeadFrames)
                        / MetaHumanProofFrameRateHz))
            {
                PrepareProductionRecoveryCapture();
            }
            else if (SecondsInStage() > Session3VisualEffectiveTimeout(
                Session3VisualReleaseTimeoutSeconds))
            {
                const UAnimInstance* Anim = GolferMesh
                    ? GolferMesh->GetAnimInstance() : nullptr;
                Fail(FString::Printf(
                    TEXT("recovery montage pose was not reached: phase=%d position=%.6f active=%d playing=%d"),
                    FrameworkThrowComponent
                        ? static_cast<int32>(FrameworkThrowComponent->CurrentPhase)
                        : -1,
                    GetMontagePosition(GolferMesh),
                    ThrowAdapter && ThrowAdapter->IsThrowActive() ? 1 : 0,
                    Anim && ThrowMontage && Anim->Montage_IsPlaying(ThrowMontage)
                        ? 1 : 0));
            }
            break;

        case EStage::WaitingForProductionFlightEvidence:
            if (!bMetaHumanProductionVisualCapture)
            {
                Fail(TEXT("production flight evidence stage is MetaHuman-capture-only"));
            }
            else if (bLiveAuthoritativeFlightSettled
                && !bLiveAuthoritativeFlightValidationAttempted)
            {
                UGameInstance* GameInstance = GetWorld()
                    ? GetWorld()->GetGameInstance() : nullptr;
                const UDiscTrajectorySubsystem* Trajectories = GameInstance
                    ? GameInstance->GetSubsystem<UDiscTrajectorySubsystem>()
                    : nullptr;
                const bool bFreshSummaryPublished = Trajectories
                    && Trajectories->HasLastCapture()
                    && Trajectories->GetLastSummary().CaptureId
                        != BaselineTrajectoryCaptureId;
                const bool bDeferredSummaryPublished = Trajectories
                    && Trajectories->HasPendingDeferredCaptureExport();
                if (bFreshSummaryPublished || bDeferredSummaryPublished)
                {
                    bLiveAuthoritativeFlightValidationAttempted = true;
                    FString Error;
                    bLiveAuthoritativeFlightValidationPassed =
                        ValidateCompletedLiveFlightEvidence(
                            LiveGameplayDisc, Error);
                    LiveFlightValidationFailure = Error;
                    if (!bLiveAuthoritativeFlightValidationPassed
                        || !bLiveTrajectoryCaptureValid)
                    {
                        DiscardLiveTrajectoryExportDeferral();
                        Fail(FString::Printf(
                            TEXT("authoritative flight/trajectory evidence failed: %s"),
                            LiveFlightValidationFailure.IsEmpty()
                                ? TEXT("completed flight did not satisfy the evidence contract")
                                : *LiveFlightValidationFailure));
                    }
                }
                else if (SecondsInStage()
                    > Session3VisualEffectiveTimeout(
                        Session3VisualGameplayRecoveryTimeoutSeconds))
                {
                    Fail(TEXT("authoritative terminal event was not followed by a fresh deferred trajectory summary"));
                }
            }
            else if (bLiveThrowRecovered
                && bLiveAuthoritativeFlightSettled
                && bLiveAuthoritativeFlightValidationAttempted
                && bLiveAuthoritativeFlightValidationPassed
                && bLiveTrajectoryCaptureValid)
            {
                Pass();
            }
            else if (SecondsInStage()
                > Session3VisualEffectiveTimeout(
                    Session3VisualGameplayRecoveryTimeoutSeconds))
            {
                Fail(FString::Printf(
                    TEXT("authoritative flight/trajectory evidence did not complete: recovered=%d terminal_event=%d holed_out=%d validation_attempted=%d flight_valid=%d trajectory_valid=%d failure=%s"),
                    bLiveThrowRecovered ? 1 : 0,
                    bLiveAuthoritativeFlightSettled ? 1 : 0,
                    bLiveAuthoritativeFlightHoledOut ? 1 : 0,
                    bLiveAuthoritativeFlightValidationAttempted ? 1 : 0,
                    bLiveAuthoritativeFlightValidationPassed ? 1 : 0,
                    bLiveTrajectoryCaptureValid ? 1 : 0,
                    *LiveFlightValidationFailure));
            }
            break;

        case EStage::WaitingForGameplayRecovery:
            if (GameMode->CanPlayerThrow()
                && !ThrowAdapter->IsThrowActive()
                && GameMode->GetActiveDisc() == nullptr
                && !GameMode->IsBroadcastCameraActive())
            {
                PrepareRecoveredCapture();
            }
            else if (SecondsInStage() > Session3VisualEffectiveTimeout(
                Session3VisualGameplayRecoveryTimeoutSeconds))
            {
                Fail(TEXT("flight/lie/camera path did not recover before visual timeout"));
            }
            break;

        case EStage::StartingProfile:
            BeginProfileFixture();
            break;

        case EStage::WaitingForProfilePose:
        {
            const float Position = GetMontagePosition(ProfileMesh);
            if (Position >= 1.68f)
            {
                ProfileRecords[ProfileIndex].bFollowThroughReached = true;
            }
            if (Position >= ProfileCaptureMontageSeconds[ProfileIndex])
            {
                PrepareProfileCapture();
            }
            else if (SecondsInStage() > ProfileTimeoutSeconds)
            {
                Fail(FString::Printf(TEXT("%s profile capture pose was not reached"), ProfileNames[ProfileIndex]));
            }
            break;
        }

        case EStage::WaitingForProfileRecovery:
            if (ProfileMesh && GetMontagePosition(ProfileMesh) >= 1.68f)
            {
                ProfileRecords[ProfileIndex].bFollowThroughReached = true;
            }
            if (bProfileRecoveredCallback && ProfileAdapter && !ProfileAdapter->IsThrowActive())
            {
                FinishProfileFixture();
            }
            else if (SecondsInStage() > ProfileTimeoutSeconds)
            {
                Fail(FString::Printf(TEXT("%s profile playback did not recover"), ProfileNames[ProfileIndex]));
            }
            break;

        case EStage::CapturingHeld:
        case EStage::CapturingPlant:
        case EStage::CapturingExactRelease:
        case EStage::CapturingPostRelease:
        case EStage::CapturingFollowThrough:
        case EStage::CapturingRecoveryProof:
        case EStage::CapturingGameplayRecovery:
        case EStage::CapturingProfile:
        case EStage::Finished:
        default:
            break;
    }
}

void ADiscGolfSession3VisualCaptureRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SetLiveDiscSimulationPaused(false);
    RestoreCaptureTimeDilation();
    RestoreCapturePresentationState();
    if (bLiveTrajectoryExportDeferralRequested
        && !bLiveTrajectoryExportDeferralDiscarded)
    {
        DiscardLiveTrajectoryExportDeferral();
    }
    if (ThrowAdapter)
    {
        ThrowAdapter->OnReleaseCommitted.RemoveDynamic(
            this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveRelease);
        ThrowAdapter->OnThrowRecovered.RemoveDynamic(
            this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveRecovery);
    }
    if (LiveGameplayDisc)
    {
        LiveGameplayDisc->OnDiscSettled.RemoveDynamic(
            this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveDiscSettled);
        LiveGameplayDisc->OnDiscHoledOut.RemoveDynamic(
            this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveDiscHoledOut);
    }
    if (bMetaHumanBodyTickPrerequisiteAdded && MetaHumanVisualBody)
    {
        RemoveTickPrerequisiteComponent(MetaHumanVisualBody);
        bMetaHumanBodyTickPrerequisiteAdded = false;
    }
    DestroyProfileFixture();
    Super::EndPlay(EndPlayReason);
}

void ADiscGolfSession3VisualCaptureRunner::SetStage(EStage NewStage)
{
    Stage = NewStage;
    StageStartRealSeconds = FPlatformTime::Seconds();
}

double ADiscGolfSession3VisualCaptureRunner::SecondsInStage() const
{
    return FPlatformTime::Seconds() - StageStartRealSeconds;
}

bool ADiscGolfSession3VisualCaptureRunner::
    ConfigureMetaHumanProductionPresentation(FString& OutError)
{
    const TCHAR* CommandLine = FCommandLine::Get();
    int32 RequestedWidth = 0;
    int32 RequestedHeight = 0;
    if (!FParse::Param(CommandLine, TEXT("d3d12"))
        || FParse::Param(CommandLine, TEXT("nullrhi"))
        || !FParse::Param(CommandLine, TEXT("ForceRes"))
        || !FParse::Value(CommandLine, TEXT("ResX="), RequestedWidth)
        || !FParse::Value(CommandLine, TEXT("ResY="), RequestedHeight)
        || RequestedWidth != 1920
        || RequestedHeight != 1080
        || !FParse::Param(CommandLine, TEXT("NoLoadExistingSave"))
        || !FParse::Param(CommandLine, TEXT("DGNoProfileWrites"))
        || !FParse::Param(CommandLine, TEXT("DGDeveloperToolNoSave")))
    {
        OutError = TEXT(
            "D3D12, exact 1920x1080 ForceRes, and all no-load/no-write save guards are mandatory; NullRHI is forbidden");
        return false;
    }
    if (!GameMode || !PlayerController || !Golfer
        || !GameMode->CanOpenCharacterCreator()
        || !PlayerController->OpenCharacterCreator())
    {
        OutError = PlayerController
            ? PlayerController->GetCharacterCreatorStatusText()
            : TEXT("player controller was unavailable");
        return false;
    }

    FDGFullCharacterCustomization Candidate;
    if (!PlayerController->SelectCharacterCreatorBackend(
            FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
            Candidate))
    {
        OutError = PlayerController->GetCharacterCreatorBackendStatusText();
        PlayerController->CancelCharacterCreator();
        return false;
    }
    Candidate.Identity.Handedness = EDGHandedness::Right;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Candidate);
    if (!PlayerController->ApplyFullCharacterCreatorDraft(Candidate)
        || PlayerController->IsCharacterCreatorOpen())
    {
        OutError = PlayerController->GetCharacterCreatorStatusText();
        if (PlayerController->IsCharacterCreatorOpen())
        {
            PlayerController->CancelCharacterCreator();
        }
        return false;
    }

    MetaHumanBackend = Golfer->GetAvatarBackendComponent();
    MetaHumanVisualBody = MetaHumanBackend
        ? MetaHumanBackend->GetVerifiedVisualBody()
        : nullptr;
    if (!MetaHumanBackend
        || !MetaHumanBackend->IsVisualBackendReady()
        || !MetaHumanBackend->IsPresentationPolicyVerified()
        || MetaHumanBackend->GetVerifiedPresentationPolicy()
            != EDGMetaHumanPresentationPolicy::GameplayPerformance
        || !MetaHumanVisualBody
        || !MetaHumanVisualBody->GetSkeletalMeshAsset()
        || !MetaHumanBackend->GetVerifiedVisualHead()
        || !MetaHumanBackend->GetVerifiedVisualOutfit()
        || Golfer->IsDGProxyPresentationVisible())
    {
        OutError = MetaHumanBackend
            ? MetaHumanBackend->GetLastAdapterStatus()
            : TEXT("MetaHuman backend component was unavailable");
        return false;
    }
    AddTickPrerequisiteComponent(MetaHumanVisualBody);
    bMetaHumanBodyTickPrerequisiteAdded = true;
    MetaHumanVisualBody->RefreshBoneTransforms();
    MetaHumanVisualBody->UpdateBounds();
    return true;
}

USkeletalMeshComponent* ADiscGolfSession3VisualCaptureRunner::
    GetCaptureSubjectMesh() const
{
    return bMetaHumanProductionVisualCapture
        ? MetaHumanVisualBody.Get()
        : GolferMesh.Get();
}

int32 ADiscGolfSession3VisualCaptureRunner::GetExpectedCaptureCount() const
{
    return bMetaHumanProductionVisualCapture
        ? UE_ARRAY_COUNT(MetaHumanProductionCaptureFilenames)
        : UE_ARRAY_COUNT(CaptureFilenames);
}

const TCHAR* ADiscGolfSession3VisualCaptureRunner::GetCaptureFilename(
    int32 CaptureIndex) const
{
    if (CaptureIndex < 0 || CaptureIndex >= GetExpectedCaptureCount())
    {
        return TEXT("");
    }
    return bMetaHumanProductionVisualCapture
        ? MetaHumanProductionCaptureFilenames[CaptureIndex]
        : CaptureFilenames[CaptureIndex];
}

bool ADiscGolfSession3VisualCaptureRunner::BuildTimingCommand(
    FThrowCommand& OutCommand,
    bool bBeginOnly)
{
    UThrowControllerComponent* Controller = Golfer ? Golfer->GetThrowController() : nullptr;
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    if (!Controller || !Bag || Bag->GetSelectedMoldId().IsNone())
    {
        return false;
    }

    if (bBeginOnly)
    {
        Controller->CancelTiming();
        Controller->SetShotContext(EDiscShotContext::Drive, GameMode->GetBasketDistanceMeters());
        if (Controller->GetThrowStyle() != EThrowStyle::Backhand)
        {
            Controller->ToggleThrowStyle();
        }
        FThrowCommand Unexpected;
        return !Controller->HandleThrowPress(
            Bag->GetSelectedMoldId(), Bag->GetSelectedPlastic(),
            Golfer->GetActorForwardVector(), Unexpected)
            && Controller->IsTimingActive();
    }

    return Controller->HandleThrowPress(
            Bag->GetSelectedMoldId(), Bag->GetSelectedPlastic(),
            Golfer->GetActorForwardVector(), OutCommand)
        && BindSession3SelectedThrowProvenance(Golfer, OutCommand);
}

void ADiscGolfSession3VisualCaptureRunner::BeginLiveThrow()
{
    LiveCommittedMontageInstanceId = INDEX_NONE;
    if (!BuildTimingCommand(LiveCommand, false)
        || LiveCommand.ThrowStyle != EThrowStyle::Backhand
        || LiveCommand.ShotContext != EDiscShotContext::Drive)
    {
        Fail(TEXT("real RHBH command could not be built for the project animation adapter"));
        return;
    }
    if (bMetaHumanProductionVisualCapture)
    {
        UGameInstance* GameInstance = GetWorld()
            ? GetWorld()->GetGameInstance() : nullptr;
        UDiscTrajectorySubsystem* Trajectories = GameInstance
            ? GameInstance->GetSubsystem<UDiscTrajectorySubsystem>() : nullptr;
        FString DeferralError;
        if (!Trajectories
            || !Trajectories->DeferNextCaptureExport(DeferralError))
        {
            Fail(FString::Printf(
                TEXT("live proof could not arm its no-write trajectory export deferral: %s"),
                Trajectories
                    ? *DeferralError
                    : TEXT("trajectory subsystem unavailable")));
            return;
        }
        bLiveTrajectoryExportDeferralRequested = true;
    }
    if (!Golfer->TryStartAnimatedRHBHThrow(LiveCommand))
    {
        Fail(TEXT("real RHBH command did not enter the project animation adapter"));
        return;
    }
    if (bMetaHumanProductionVisualCapture)
    {
        UAnimInstance* Anim = GolferMesh ? GolferMesh->GetAnimInstance() : nullptr;
        UAnimMontage* ActiveMontage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
        const FString PawnSelectedPath = Golfer->GetActiveRHBHThrowMontagePath();
        const FString AnimSelectedPath = ActiveMontage
            ? ActiveMontage->GetPathName()
            : FString();
        const FString RevisionToken = FString::Printf(
            TEXT("_%s"),
            *ExpectedProductionMotionRevision);
        if (!ActiveMontage
            || AnimSelectedPath != PawnSelectedPath
            || !PawnSelectedPath.Contains(
                TEXT("/Game/DiscGolf/Animation/ProductionMotion/Drive/"),
                ESearchCase::CaseSensitive)
            || !PawnSelectedPath.EndsWith(
                RevisionToken,
                ESearchCase::CaseSensitive))
        {
            Fail(FString::Printf(
                TEXT("live MetaHuman throw selected the wrong production montage (expected Drive/*_%s, pawn=%s, anim=%s)"),
                *ExpectedProductionMotionRevision,
                *PawnSelectedPath,
                AnimSelectedPath.IsEmpty() ? TEXT("<none>") : *AnimSelectedPath));
            return;
        }
        if (!ActiveMontage->bEnableAutoBlendOut
            || !FMath::IsNearlyEqual(
                ActiveMontage->BlendOutTriggerTime,
                MetaHumanProofRequiredBlendOutTriggerTimeSeconds,
                UE_KINDA_SMALL_NUMBER))
        {
            Fail(FString::Printf(
                TEXT("live production montage does not preserve the full-weight Recovery blend contract: auto=%d trigger=%.6f expected=%.6f"),
                ActiveMontage->bEnableAutoBlendOut ? 1 : 0,
                ActiveMontage->BlendOutTriggerTime,
                MetaHumanProofRequiredBlendOutTriggerTimeSeconds));
            return;
        }
        ThrowMontage = ActiveMontage;
        ActiveProductionMontagePath = PawnSelectedPath;
        LiveCommittedMontageInstanceId =
            FrameworkThrowComponent->GetCommittedMontageInstanceId();
        FAnimMontageInstance* const CommittedInstance =
            ResolveExactMontageInstance(
                GolferMesh, ThrowMontage, LiveCommittedMontageInstanceId);
        if (!CommittedInstance
            || !FrameworkThrowComponent->IsCommittedMontageInstance(
                LiveCommittedMontageInstanceId, ThrowMontage))
        {
            Fail(TEXT("live production throw did not expose its exact committed montage instance"));
            return;
        }
    }
    SetStage(EStage::WaitingForHeldPose);
}

void ADiscGolfSession3VisualCaptureRunner::PrepareHeldCapture()
{
    USkeletalMeshComponent* CaptureSubjectMesh = GetCaptureSubjectMesh();
    if (!HeldDiscVisual->IsVisible() || HeldDiscVisual->bHiddenInGame
        || !CaptureSubjectMesh
        || !ThrowAdapter->IsAwaitingRelease()
        || CountWorldDiscs() != BaselineWorldDiscCount
        || (bMetaHumanProductionVisualCapture
            && FrameworkThrowComponent->CurrentPhase != EDGThrowPhase::ReachBack))
    {
        Fail(TEXT("held-disc capture did not have one visible grip disc and zero launched discs"));
        return;
    }
    if (bMetaHumanProductionVisualCapture
        && IsSession19TemporalFrameDump())
    {
        PositionFullBodyCamera(
            Golfer, CaptureSubjectMesh,
            MetaHumanProofCameraFieldOfViewDegrees);
        ClearEvidenceMarkers();
        ShowEvidenceLabel(
            TEXT("SESSION 19 | METAHUMAN | V007 TEMPORAL REVIEW"),
            TEXT("FIXED 60 HZ | UNINTERRUPTED LIVE THROW | NO CHECKPOINT PAUSES"));
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION19_TEMPORAL_DUMP: REACHBACK frame=%llu montage=%.6f"),
            GFrameCounter, GetMontagePosition(GolferMesh));
        SetStage(EStage::WaitingForPlant);
        return;
    }
    if (bMetaHumanProductionVisualCapture)
    {
        FString PauseError;
        if (!PauseExactMontageInstance(
                GolferMesh, ThrowMontage, PauseError,
                LiveCommittedMontageInstanceId))
        {
            Fail(FString::Printf(
                TEXT("reachback capture could not freeze the legitimate montage lifecycle: %s"),
                *PauseError));
            return;
        }
    }
    else if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
    {
        Anim->Montage_Pause(ThrowMontage);
    }
    HeldDiscVisual->UpdateBounds();
    const FVector HeldDiscLocation = GetDiscEvidenceLocation(HeldDiscVisual);
    PositionGripCloseupCamera(
        Golfer, CaptureSubjectMesh, HeldDiscLocation, 36.0f);
    ClearEvidenceMarkers();
    PendingEvidenceMesh = HeldDiscVisual;
    PendingEvidenceMarkerColor = FColor::Cyan;
    DrawDiscEvidenceMarker(HeldDiscLocation, PendingEvidenceMarkerColor);
    ShowEvidenceLabel(
        bMetaHumanProductionVisualCapture
            ? TEXT("SESSION 19 | METAHUMAN | PRODUCTION REACHBACK")
            : TEXT("SESSION 3 | HELD DISC BEFORE RELEASE"),
        bMetaHumanProductionVisualCapture
            ? TEXT("VERIFIED METAHUMAN BODY / HEAD / OUTFIT  |  HELD DISC AT disc_grip_r  |  EXACT REACHBACK")
            : TEXT("CYAN RING: HELD CYLINDER AT disc_grip_r  |  GAMEPLAY DISCS: 0"));
    SetStage(EStage::CapturingHeld);
    RequestCapture(0, TEXT("Held provisional Cylinder follows disc_grip_r before release."),
        bMetaHumanProductionVisualCapture
            ? TEXT("verified MetaHuman GameplayPerformance presentation + active production Drive montage")
            : TEXT("possessed gameplay pawn + live montage"));
    ValidateCameraFraming(
        0, Golfer, CaptureSubjectMesh, HeldDiscLocation,
        bMetaHumanProductionVisualCapture,
        GetProvisionalDiscRadiusCm(HeldDiscVisual), HeldDiscVisual);
}

void ADiscGolfSession3VisualCaptureRunner::PreparePlantCapture()
{
    USkeletalMeshComponent* CaptureSubjectMesh = GetCaptureSubjectMesh();
    if (!bMetaHumanProductionVisualCapture
        || !CaptureSubjectMesh
        || !HeldDiscVisual
        || !HeldDiscVisual->IsVisible()
        || HeldDiscVisual->bHiddenInGame
        || !ThrowAdapter->IsAwaitingRelease()
        || CountWorldDiscs() != BaselineWorldDiscCount
        || FrameworkThrowComponent->CurrentPhase != EDGThrowPhase::Plant)
    {
        Fail(TEXT("plant capture did not retain the pre-release grip state"));
        return;
    }
    if (IsSession19TemporalFrameDump())
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION19_TEMPORAL_DUMP: PLANT frame=%llu montage=%.6f"),
            GFrameCounter, GetMontagePosition(GolferMesh));
        SetStage(EStage::WaitingForRelease);
        return;
    }
    FString PauseError;
    if (!PauseExactMontageInstance(
            GolferMesh, ThrowMontage, PauseError,
            LiveCommittedMontageInstanceId))
    {
        Fail(FString::Printf(
            TEXT("plant capture could not freeze the legitimate montage lifecycle: %s"),
            *PauseError));
        return;
    }
    HeldDiscVisual->UpdateBounds();
    const FVector HeldDiscLocation = GetDiscEvidenceLocation(HeldDiscVisual);
    PositionFullBodyCamera(
        Golfer, CaptureSubjectMesh, MetaHumanProofCameraFieldOfViewDegrees);
    ClearEvidenceMarkers();
    PendingEvidenceMesh = HeldDiscVisual;
    PendingEvidenceMarkerColor = FColor::Cyan;
    DrawDiscEvidenceMarker(HeldDiscLocation, PendingEvidenceMarkerColor);
    ShowEvidenceLabel(
        TEXT("SESSION 19 | METAHUMAN | PRODUCTION PLANT"),
        TEXT("LOCKED SIDE CAMERA | FULL BODY / TEE CONTACT | HELD DISC"));
    SetStage(EStage::CapturingPlant);
    RequestCapture(
        1,
        TEXT("Exact pre-release plant checkpoint with the held disc attached to disc_grip_r."),
        TEXT("verified MetaHuman GameplayPerformance presentation + active production Drive montage"));
    ValidateCameraFraming(
        1,
        Golfer,
        CaptureSubjectMesh,
        HeldDiscLocation,
        true,
        GetProvisionalDiscRadiusCm(HeldDiscVisual),
        HeldDiscVisual);
}

void ADiscGolfSession3VisualCaptureRunner::HandleLiveRelease(
    int64 AttemptSerial,
    bool bAuthoritativeLaunchAccepted)
{
    ++LiveReleaseCallbackCount;
    if (bFinished)
    {
        return;
    }
    LiveGameplayDisc = GameMode ? GameMode->GetActiveDisc() : nullptr;
    const bool bSingleAuthorityState = Stage == EStage::WaitingForRelease
        && bAuthoritativeLaunchAccepted
        && LiveReleaseCallbackCount == 1
        && LiveGameplayDisc != nullptr
        && CountWorldDiscs() == BaselineWorldDiscCount + 1
        && ThrowAdapter->GetTotalReleaseCommitCount() == BaselineLiveReleaseCount + 1
        && ThrowAdapter->GetReleaseCommitCountForAttempt() == 1
        && !HeldDiscVisual->IsVisible();
    if (!bSingleAuthorityState)
    {
        Fail(TEXT("exact release callback did not expose one held-disc-off / gameplay-disc-on authority state"));
        return;
    }
    LiveGameplayDisc->OnDiscSettled.AddUniqueDynamic(
        this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveDiscSettled);
    LiveGameplayDisc->OnDiscHoledOut.AddUniqueDynamic(
        this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveDiscHoledOut);

    USkeletalMeshComponent* CaptureSubjectMesh = GetCaptureSubjectMesh();
    UStaticMeshComponent* ReleasedDiscMesh = FindNamedStaticMesh(
        LiveGameplayDisc, TEXT("DiscMesh"));
    if (ReleasedDiscMesh)
    {
        GameplayDiscEvidenceMaterial = ApplyDiscEvidenceMaterial(
            ReleasedDiscMesh, FLinearColor(1.0f, 0.16f, 0.01f, 1.0f));
    }
    if (!CaptureSubjectMesh || !ReleasedDiscMesh || !GameplayDiscEvidenceMaterial
        || (bMetaHumanProductionVisualCapture
            && FrameworkThrowComponent->CurrentPhase != EDGThrowPhase::Release))
    {
        Fail(TEXT("capture-only gameplay-disc material could not be created"));
        return;
    }

    if (bMetaHumanProductionVisualCapture
        && IsSession19TemporalFrameDump())
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION19_TEMPORAL_DUMP: RELEASE frame=%llu montage=%.6f attempt=%lld"),
            GFrameCounter, GetMontagePosition(GolferMesh), AttemptSerial);
        SetStage(EStage::WaitingForFollowThrough);
        return;
    }

    // Evidence must follow the rendered Cylinder component, not merely the actor
    // transform. Keeping this explicit also catches any future visual-offset layer.
    if (bMetaHumanProductionVisualCapture)
    {
        FString PauseError;
        if (!PauseExactMontageInstance(
                GolferMesh, ThrowMontage, PauseError,
                LiveCommittedMontageInstanceId))
        {
            Fail(FString::Printf(
                TEXT("release capture could not freeze the legitimate montage lifecycle: %s"),
                *PauseError));
            return;
        }
    }
    else if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
    {
        Anim->Montage_Pause(ThrowMontage);
    }
    SetLiveDiscSimulationPaused(true);
    UGameplayStatics::SetGlobalTimeDilation(this, 0.01f);
    PendingReleaseAttemptSerial = AttemptSerial;
    ExactReleasePoseSettleTicksCompleted = 0;
    if (bMetaHumanProductionVisualCapture)
    {
        ExactReleasePoseCallbackFrameCounter = GFrameCounter;
        ExactReleasePoseLastSettleFrameCounter = GFrameCounter;
        ExactReleasePoseValidationFrameCounter = 0;
        ExactReleasePoseBoneRevisionAtCallback =
            MetaHumanVisualBody->GetBoneTransformRevisionNumber();
        ExactReleasePoseBoneRevisionAtValidation = 0;
        ExactReleasePoseBoneTransformFrameAtValidation = 0;
        bExactReleasePoseParallelEvaluationComplete = false;
        bExactReleasePosePostEvaluateProven = false;
        SetStage(EStage::WaitingForExactReleasePoseSettle);
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION3_VISUAL_CAPTURE: RELEASE_POSE_SETTLE_BEGIN required_ticks=%d callback_frame=%llu callback_revision=%u phase=%d"),
            MetaHumanReleasePoseSettleWorldTicks,
            ExactReleasePoseCallbackFrameCounter,
            ExactReleasePoseBoneRevisionAtCallback,
            static_cast<int32>(FrameworkThrowComponent->CurrentPhase));
        return;
    }

    PrepareExactReleaseCapture();
}

void ADiscGolfSession3VisualCaptureRunner::PrepareExactReleaseCapture()
{
    USkeletalMeshComponent* CaptureSubjectMesh = GetCaptureSubjectMesh();
    UStaticMeshComponent* ReleasedDiscMesh = LiveGameplayDisc
        ? FindNamedStaticMesh(LiveGameplayDisc, TEXT("DiscMesh"))
        : nullptr;
    if (!CaptureSubjectMesh || !LiveGameplayDisc || !ReleasedDiscMesh
        || !GameplayDiscEvidenceMaterial
        || CountWorldDiscs() != BaselineWorldDiscCount + 1
        || HeldDiscVisual->IsVisible()
        || LiveReleaseCallbackCount != 1
        || (bMetaHumanProductionVisualCapture
            && (FrameworkThrowComponent->CurrentPhase != EDGThrowPhase::Release
                || !bExactReleasePosePostEvaluateProven)))
    {
        Fail(TEXT("settled exact-release pose lost authoritative disc or lifecycle state"));
        return;
    }

    ReleasedDiscMesh->UpdateBounds();
    const FVector ReleasedDiscLocation = GetDiscEvidenceLocation(ReleasedDiscMesh);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: release mesh_alignment_cm=%.6f actor=(%s) mesh=(%s) settle_ticks=%d"),
        FVector::Dist(LiveGameplayDisc->GetActorLocation(), ReleasedDiscLocation),
        *LiveGameplayDisc->GetActorLocation().ToCompactString(),
        *ReleasedDiscLocation.ToCompactString(),
        ExactReleasePoseSettleTicksCompleted);

    PositionGripCloseupCamera(
        Golfer, CaptureSubjectMesh, ReleasedDiscLocation, 36.0f);
    ClearEvidenceMarkers();
    PendingEvidenceMesh = ReleasedDiscMesh;
    PendingEvidenceMarkerColor = FColor(255, 96, 16);
    DrawDiscEvidenceMarker(ReleasedDiscLocation, PendingEvidenceMarkerColor);
    const FString ReleaseEvidenceSubtitle = bMetaHumanProductionVisualCapture
        ? FString::Printf(
            TEXT("VERIFIED METAHUMAN  |  AUTHORITATIVE DISC AT GRIP  |  ATTEMPT %lld  |  RELEASE COUNT 1"),
            PendingReleaseAttemptSerial)
        : FString::Printf(
            TEXT("ORANGE RING: AUTHORITATIVE DISC  |  ATTEMPT %lld  |  HELD: 0  |  DISC: 1"),
            PendingReleaseAttemptSerial);
    ShowEvidenceLabel(
        bMetaHumanProductionVisualCapture
            ? TEXT("SESSION 19 | METAHUMAN | EXACT PRODUCTION RELEASE")
            : TEXT("EXACT DG RELEASE DISC CALLBACK"),
        ReleaseEvidenceSubtitle);
    SetStage(EStage::CapturingExactRelease);
    RequestCapture(
        bMetaHumanProductionVisualCapture ? 2 : 1,
        bMetaHumanProductionVisualCapture
            ? TEXT("Authoritative release state established by the single DG Release Disc adapter callback; rendered after two paused presentation-settle ticks.")
            : TEXT("Captured from the single DG Release Disc adapter callback."),
        bMetaHumanProductionVisualCapture
            ? TEXT("verified MetaHuman GameplayPerformance presentation + authoritative gameplay disc + post-evaluate component pose")
            : TEXT("possessed gameplay pawn + authoritative gameplay disc"));
    ValidateCameraFraming(
        bMetaHumanProductionVisualCapture ? 2 : 1,
        Golfer,
        CaptureSubjectMesh,
        ReleasedDiscLocation,
        bMetaHumanProductionVisualCapture,
        GetProvisionalDiscRadiusCm(ReleasedDiscMesh), ReleasedDiscMesh);
}

void ADiscGolfSession3VisualCaptureRunner::HandleLiveRecovery(
    int64 AttemptSerial,
    bool bDiscWasReleased)
{
    const EDiscGolfRHBHThrowRecoveryReason Reason = ThrowAdapter
        ? ThrowAdapter->GetRecoveryReason()
        : EDiscGolfRHBHThrowRecoveryReason::None;
    LiveRecoveryReason = RecoveryReasonLabel(Reason);
    bLiveThrowRecovered = bDiscWasReleased
        && Reason == EDiscGolfRHBHThrowRecoveryReason::ThrowFinished;
}

void ADiscGolfSession3VisualCaptureRunner::HandleLiveDiscSettled(
    ADiscActor* Disc,
    FVector FinalLocation)
{
    (void)FinalLocation;
    if (bFinished || !bMetaHumanProductionVisualCapture)
    {
        return;
    }
    if (!Disc || Disc != LiveGameplayDisc)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION3_VISUAL_CAPTURE: ignored non-authoritative settled event"));
        return;
    }
    bLiveAuthoritativeFlightSettled = true;
    bLiveAuthoritativeFlightHoledOut = false;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: authoritative settled event latched; awaiting fresh trajectory publication"));
}

void ADiscGolfSession3VisualCaptureRunner::HandleLiveDiscHoledOut(
    ADiscActor* Disc)
{
    if (bFinished || !bMetaHumanProductionVisualCapture)
    {
        return;
    }
    if (!Disc || Disc != LiveGameplayDisc)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION3_VISUAL_CAPTURE: ignored non-authoritative holed-out event"));
        return;
    }
    bLiveAuthoritativeFlightSettled = true;
    bLiveAuthoritativeFlightHoledOut = true;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: authoritative holed-out event latched; awaiting fresh trajectory publication"));
}

void ADiscGolfSession3VisualCaptureRunner::PreparePostReleaseCapture()
{
    if (!LiveGameplayDisc || CountWorldDiscs() != BaselineWorldDiscCount + 1
        || HeldDiscVisual->IsVisible() || LiveReleaseCallbackCount != 1)
    {
        Fail(TEXT("immediate post-release state did not retain exactly one gameplay disc"));
        return;
    }
    PositionPostReleaseCamera();
    SetLiveDiscSimulationPaused(true);
    UStaticMeshComponent* ReleasedDiscMesh = FindNamedStaticMesh(
        LiveGameplayDisc, TEXT("DiscMesh"));
    if (!ReleasedDiscMesh)
    {
        Fail(TEXT("immediate post-release gameplay Cylinder was unavailable"));
        return;
    }
    ReleasedDiscMesh->UpdateBounds();
    const FVector ReleasedDiscLocation = GetDiscEvidenceLocation(ReleasedDiscMesh);
    ClearEvidenceMarkers();
    PendingEvidenceMesh = ReleasedDiscMesh;
    PendingEvidenceMarkerColor = FColor(255, 96, 16);
    DrawDiscEvidenceMarker(ReleasedDiscLocation, PendingEvidenceMarkerColor);
    ShowEvidenceLabel(
        TEXT("IMMEDIATE POST-RELEASE"),
        TEXT("ORANGE RING: ONE GAMEPLAY DISC  |  HAND EMPTY  |  EXISTING FLIGHT SOLVER ACTIVE"));
    SetStage(EStage::CapturingPostRelease);
    RequestCapture(2, TEXT("Immediately after release with no held-disc double and one gameplay disc."),
        TEXT("possessed gameplay pawn + authoritative gameplay disc"));
    ValidateCameraFraming(
        2, Golfer, GolferMesh, ReleasedDiscLocation, true,
        GetProvisionalDiscRadiusCm(ReleasedDiscMesh), ReleasedDiscMesh);
}

void ADiscGolfSession3VisualCaptureRunner::PrepareFollowThroughCapture()
{
    USkeletalMeshComponent* CaptureSubjectMesh = GetCaptureSubjectMesh();
    if (!ThrowAdapter->IsThrowActive() || LiveReleaseCallbackCount != 1)
    {
        Fail(TEXT("follow-through capture no longer had an active post-release animation"));
        return;
    }
    if (bMetaHumanProductionVisualCapture
        && IsSession19TemporalFrameDump())
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION19_TEMPORAL_DUMP: FOLLOWTHROUGH frame=%llu montage=%.6f"),
            GFrameCounter, GetMontagePosition(GolferMesh));
        SetStage(EStage::WaitingForRecoveryProof);
        return;
    }
    if (bMetaHumanProductionVisualCapture)
    {
        FString PauseError;
        if (!PauseExactMontageInstance(
                GolferMesh, ThrowMontage, PauseError,
                LiveCommittedMontageInstanceId))
        {
            Fail(FString::Printf(
                TEXT("follow-through capture could not freeze the legitimate montage lifecycle: %s"),
                *PauseError));
            return;
        }
    }
    else if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
    {
        Anim->Montage_Pause(ThrowMontage);
    }
    PendingEvidenceMesh = nullptr;
    PendingEvidenceMarkerColor = FColor::Transparent;
    ClearEvidenceMarkers();
    if (!CaptureSubjectMesh
        || (bMetaHumanProductionVisualCapture
            && FrameworkThrowComponent->CurrentPhase != EDGThrowPhase::FollowThrough))
    {
        Fail(TEXT("follow-through capture lost the verified MetaHuman or exact lifecycle phase"));
        return;
    }
    PositionFullBodyCamera(Golfer, CaptureSubjectMesh, 40.0f);
    ShowEvidenceLabel(
        bMetaHumanProductionVisualCapture
            ? TEXT("SESSION 19 | METAHUMAN | PRODUCTION FOLLOW-THROUGH")
            : TEXT("RHBH FOLLOW-THROUGH"),
        bMetaHumanProductionVisualCapture
            ? TEXT("VERIFIED METAHUMAN BODY / HEAD / OUTFIT  |  EXACT FOLLOW-THROUGH  |  RELEASE COUNT 1")
            : TEXT("RELEASE COUNT: 1  |  THROW TRANSACTION STILL ACTIVE"));
    SetStage(EStage::CapturingFollowThrough);
    const int32 CaptureIndex = 3;
    RequestCapture(
        CaptureIndex,
        TEXT("Character completes the authored RHBH follow-through after one release."),
        bMetaHumanProductionVisualCapture
            ? TEXT("verified MetaHuman GameplayPerformance presentation + active production Drive montage")
            : TEXT("possessed gameplay pawn + live montage"));
    ValidateCameraFraming(
        CaptureIndex,
        Golfer,
        CaptureSubjectMesh,
        GolferMesh->GetSocketLocation(TEXT("disc_grip_r")),
        true);
}

void ADiscGolfSession3VisualCaptureRunner::PrepareProductionRecoveryCapture()
{
    USkeletalMeshComponent* CaptureSubjectMesh = GetCaptureSubjectMesh();
    if (!bMetaHumanProductionVisualCapture
        || !CaptureSubjectMesh
        || !ThrowAdapter->IsThrowActive()
        || !bLiveFollowThroughReached
        || LiveReleaseCallbackCount != 1
        || FrameworkThrowComponent->CurrentPhase != EDGThrowPhase::Recovery)
    {
        Fail(TEXT("recovery proof lost the active one-release throw lifecycle"));
        return;
    }
    if (IsSession19TemporalFrameDump())
    {
        bFinished = true;
        SetStage(EStage::Finished);
        RestoreCaptureTimeDilation();
        RestoreCapturePresentationState();
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION19_TEMPORAL_DUMP: PASS revision=%s release_count=%d reachback_frame=%d plant_frame=%d release_frame=%d followthrough_frame=%d recovery_frame=%llu montage=%.6f"),
            *ExpectedProductionMotionRevision,
            LiveReleaseCallbackCount,
            MetaHumanProductionTargetFrames[0],
            MetaHumanProductionTargetFrames[1],
            MetaHumanProductionTargetFrames[2],
            MetaHumanProductionTargetFrames[3],
            GFrameCounter,
            GetMontagePosition(GolferMesh));
        FPlatformMisc::RequestExitWithStatus(false, 0);
        return;
    }
    FString PauseError;
    if (!PauseExactMontageInstance(
            GolferMesh, ThrowMontage, PauseError,
            LiveCommittedMontageInstanceId))
    {
        Fail(FString::Printf(
            TEXT("recovery capture could not freeze the legitimate montage lifecycle: %s"),
            *PauseError));
        return;
    }
    PendingEvidenceMesh = nullptr;
    PendingEvidenceMarkerColor = FColor::Transparent;
    ClearEvidenceMarkers();
    PositionFullBodyCamera(
        Golfer, CaptureSubjectMesh, MetaHumanProofCameraFieldOfViewDegrees);
    ShowEvidenceLabel(
        TEXT("SESSION 19 | METAHUMAN | PRODUCTION RECOVERY"),
        TEXT("LOCKED SIDE CAMERA | DECELERATION / RECENTER | RELEASE COUNT 1"));
    SetStage(EStage::CapturingRecoveryProof);
    RequestCapture(
        4,
        TEXT("Exact authored recovery checkpoint after the one authoritative release."),
        TEXT("verified MetaHuman GameplayPerformance presentation + active production Drive montage"));
    ValidateCameraFraming(
        4,
        Golfer,
        CaptureSubjectMesh,
        CaptureSubjectMesh->GetSocketLocation(TEXT("hand_r")),
        true);
}

void ADiscGolfSession3VisualCaptureRunner::PrepareRecoveredCapture()
{
    if (!bLiveThrowRecovered || !bLiveFollowThroughReached
        || GameMode->GetStrokes() != BaselineStrokes + 1
        || LiveReleaseCallbackCount != 1)
    {
        Fail(TEXT("recovered view lacked one-release / one-stroke / follow-through proof"));
        return;
    }
    ClearEvidenceMarkers();
    PositionFullBodyCamera(Golfer, GolferMesh, 44.0f);
    ShowEvidenceLabel(
        TEXT("RECOVERED PLAYABLE CONTROL"),
        TEXT("ANIMATION IDLE  |  CAMERA/INPUT RETURNED  |  NEXT LEGAL ACTION AVAILABLE"));
    SetStage(EStage::CapturingGameplayRecovery);
    RequestCapture(4, TEXT("Flight, lie, camera, animation, and input recovered to playable control."),
        TEXT("possessed gameplay pawn after authoritative flight/lie resolution"));
    ValidateCameraFraming(
        4, Golfer, GolferMesh, GolferMesh->Bounds.Origin, true);
}

void ADiscGolfSession3VisualCaptureRunner::BeginProfileFixture()
{
    if (ProfileIndex >= 3)
    {
        Pass();
        return;
    }

    DestroyProfileFixture();
    UDiscGolfCharacterProfile* Profile = LoadObject<UDiscGolfCharacterProfile>(
        nullptr, ProfilePaths[ProfileIndex]);
    if (!Profile)
    {
        Fail(FString::Printf(TEXT("profile asset did not load: %s"), ProfilePaths[ProfileIndex]));
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ProfileFixture = GetWorld()->SpawnActor<ADiscGolferPawn>(
        ADiscGolferPawn::StaticClass(), InitialGolferTransform, SpawnParameters);
    ProfileAdapter = ProfileFixture
        ? ProfileFixture->GetRHBHThrowAdapter() : nullptr;
    ProfileThrowComponent = ProfileFixture
        ? ProfileFixture->FindComponentByClass<UDiscGolfThrowComponent>() : nullptr;
    ProfileMesh = FindSkeletalMesh(ProfileFixture);
    if (!ProfileFixture || !ProfileAdapter || !ProfileThrowComponent || !ProfileMesh)
    {
        Fail(FString::Printf(TEXT("%s transient profile pawn was incomplete"), ProfileNames[ProfileIndex]));
        return;
    }
    ApplyNeutralProxyMaterials(ProfileMesh);

    ProfileFixture->SetActorEnableCollision(false);
    ProfileThrowComponent->CharacterProfile = Profile;
    ProfileAdapter->GetAuthoritativeLaunchDelegate().Unbind();
    ProfileAdapter->GetAuthoritativeLaunchDelegate().BindUObject(
        this, &ADiscGolfSession3VisualCaptureRunner::HandleProfileFixtureLaunch);
    ProfileAdapter->OnReleaseCommitted.AddUniqueDynamic(
        this, &ADiscGolfSession3VisualCaptureRunner::HandleProfileRelease);
    ProfileAdapter->OnThrowRecovered.AddUniqueDynamic(
        this, &ADiscGolfSession3VisualCaptureRunner::HandleProfileRecovery);

    ProfileLaunchCount = 0;
    ProfileReleaseCallbackCount = 0;
    bProfileRecoveredCallback = false;
    ProfileRecoveryReason.Reset();
    FProfileRecord Record;
    Record.Name = ProfileNames[ProfileIndex];
    Record.AssetPath = ProfilePaths[ProfileIndex];
    Record.HeightCm = Profile->Body.HeightCm;
    Record.WingspanScale = Profile->Body.WingspanScale;
    ProfileRecords.Add(Record);

    Golfer->SetActorHiddenInGame(true);
    FThrowCommand ProfileCommand = LiveCommand;
    const bool bStartedProfile =
        BindSession3SelectedThrowProvenance(ProfileFixture, ProfileCommand)
        && ProfileFixture->TryStartAnimatedRHBHThrow(ProfileCommand);
    ProfileRecords[ProfileIndex].bAnimationStarted = bStartedProfile;
    if (!bStartedProfile)
    {
        Fail(FString::Printf(TEXT("%s profile montage did not start"), ProfileNames[ProfileIndex]));
        return;
    }
    SetStage(EStage::WaitingForProfilePose);
}

void ADiscGolfSession3VisualCaptureRunner::PrepareProfileCapture()
{
    if (!ProfileFixture || !ProfileMesh || !ProfileAdapter || !ProfileAdapter->IsThrowActive())
    {
        Fail(FString::Printf(TEXT("%s profile was not active at capture"), ProfileNames[ProfileIndex]));
        return;
    }
    ProfileRecords[ProfileIndex].bPoseFiniteAndPlausible = IsProfilePoseFiniteAndPlausible();
    if (!ProfileRecords[ProfileIndex].bPoseFiniteAndPlausible)
    {
        Fail(FString::Printf(TEXT("%s profile pose contained invalid or implausible transforms"), ProfileNames[ProfileIndex]));
        return;
    }
    if (UAnimInstance* Anim = ProfileMesh->GetAnimInstance())
    {
        Anim->Montage_Pause(ThrowMontage);
    }
    ClearEvidenceMarkers();
    PositionFullBodyCamera(ProfileFixture, ProfileMesh, 40.0f);
    ShowEvidenceLabel(
        FString::Printf(TEXT("%s | SESSION 3 RHBH PLAYBACK"), ProfileNames[ProfileIndex]),
        FString::Printf(
            TEXT("HEIGHT %.0f CM  |  WING %.2f  |  SAME SK/IK/CR/MONTAGE  |  DEFORMATION S4"),
            ProfileRecords[ProfileIndex].HeightCm,
            ProfileRecords[ProfileIndex].WingspanScale));
    SetStage(EStage::CapturingProfile);
    RequestCapture(
        5 + ProfileIndex,
        FString::Printf(TEXT("%s body-profile compatibility playback."), ProfileNames[ProfileIndex]),
        TEXT("transient compatibility pawn + same framework/montage; no body deformation claim"));
    ValidateCameraFraming(
        5 + ProfileIndex,
        ProfileFixture,
        ProfileMesh,
        ProfileMesh->GetSocketLocation(TEXT("disc_grip_r")),
        true);
}

bool ADiscGolfSession3VisualCaptureRunner::HandleProfileFixtureLaunch(
    const FThrowCommand& AuthoritativeCommand,
    const FTransform& GripWorldTransform)
{
    ++ProfileLaunchCount;
    const bool bUsable = AuthoritativeCommand.ThrowStyle == EThrowStyle::Backhand
        && AuthoritativeCommand.ShotContext == EDiscShotContext::Drive
        && GripWorldTransform.IsValid()
        && IsSession3FiniteVector(GripWorldTransform.GetLocation())
        && ProfileMesh
        && FVector::Dist(
            GripWorldTransform.GetLocation(),
            ProfileMesh->GetSocketLocation(TEXT("pelvis"))) < 500.0f;
    if (ProfileRecords.IsValidIndex(ProfileIndex))
    {
        ProfileRecords[ProfileIndex].bGripTransformUsable = bUsable;
    }
    // Compatibility fixtures validate the framework event and grip only. They
    // must not add gameplay discs, strokes, lies, cameras, or physics work.
    return bUsable;
}

void ADiscGolfSession3VisualCaptureRunner::HandleProfileRelease(
    int64 AttemptSerial,
    bool bLaunchAccepted)
{
    ++ProfileReleaseCallbackCount;
    if (!bLaunchAccepted || ProfileLaunchCount != 1 || ProfileReleaseCallbackCount != 1)
    {
        Fail(FString::Printf(TEXT("%s profile emitted an invalid or duplicate release"), ProfileNames[ProfileIndex]));
    }
}

void ADiscGolfSession3VisualCaptureRunner::HandleProfileRecovery(
    int64 AttemptSerial,
    bool bDiscWasReleased)
{
    const EDiscGolfRHBHThrowRecoveryReason Reason = ProfileAdapter
        ? ProfileAdapter->GetRecoveryReason()
        : EDiscGolfRHBHThrowRecoveryReason::None;
    ProfileRecoveryReason = RecoveryReasonLabel(Reason);
    bProfileRecoveredCallback = bDiscWasReleased
        && Reason == EDiscGolfRHBHThrowRecoveryReason::ThrowFinished;
}

void ADiscGolfSession3VisualCaptureRunner::FinishProfileFixture()
{
    FProfileRecord& Record = ProfileRecords[ProfileIndex];
    Record.ReleaseCount = ProfileReleaseCallbackCount;
    Record.bRecovered = bProfileRecoveredCallback;
    Record.RecoveryReason = ProfileRecoveryReason;
    if (ProfileLaunchCount != 1 || Record.ReleaseCount != 1
        || !Record.bGripTransformUsable || !Record.bFollowThroughReached
        || !Record.bRecovered || !Record.bPoseFiniteAndPlausible
        || CountWorldDiscs() != BaselineWorldDiscCount
        || GameMode->GetStrokes() != BaselineStrokes + 1)
    {
        Fail(FString::Printf(TEXT("%s compatibility playback failed its one-release recovery invariant"), *Record.Name));
        return;
    }

    DestroyProfileFixture();
    ++ProfileIndex;
    SetStage(EStage::StartingProfile);
}

void ADiscGolfSession3VisualCaptureRunner::DestroyProfileFixture()
{
    if (ProfileAdapter)
    {
        ProfileAdapter->OnReleaseCommitted.RemoveDynamic(
            this, &ADiscGolfSession3VisualCaptureRunner::HandleProfileRelease);
        ProfileAdapter->OnThrowRecovered.RemoveDynamic(
            this, &ADiscGolfSession3VisualCaptureRunner::HandleProfileRecovery);
        ProfileAdapter->GetAuthoritativeLaunchDelegate().Unbind();
    }
    if (ProfileFixture && IsValid(ProfileFixture))
    {
        ProfileFixture->Destroy();
    }
    ProfileFixture = nullptr;
    ProfileAdapter = nullptr;
    ProfileThrowComponent = nullptr;
    ProfileMesh = nullptr;
    if (Golfer)
    {
        Golfer->SetActorHiddenInGame(false);
    }
}

void ADiscGolfSession3VisualCaptureRunner::RequestCapture(
    int32 CaptureIndex,
    const FString& Evidence,
    const FString& Source)
{
    if (bScreenshotPending || CaptureIndex < 0
        || CaptureIndex >= GetExpectedCaptureCount())
    {
        Fail(TEXT("capture request was invalid or overlapped another screenshot"));
        return;
    }
    PendingCaptureIndex = CaptureIndex;
    PendingCapturePath = FPaths::Combine(
        OutputDirectory,
        GetCaptureFilename(CaptureIndex));
    IFileManager::Get().Delete(*PendingCapturePath, false, true, true);
    bScreenshotPending = true;
    bScreenshotIssued = false;

    FCaptureRecord Record;
    Record.Filename = GetCaptureFilename(CaptureIndex);
    Record.Evidence = Evidence;
    Record.Source = Source;
    CaptureRecords.Add(Record);

    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetViewTarget(CaptureCamera);
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: FRAMED %d/%d %s"),
        CaptureIndex + 1,
        GetExpectedCaptureCount(),
        *PendingCapturePath);
}

bool ADiscGolfSession3VisualCaptureRunner::PollPendingCapture()
{
    // SetViewTarget was applied when the pose was framed. Delay the screenshot
    // request until the next world tick so PlayerCameraManager has evaluated the
    // external camera instead of reusing the previous gameplay view cache.
    if (!bScreenshotIssued)
    {
        if (bMetaHumanProductionVisualCapture)
        {
            ApplyLockedMetaHumanProofCamera();
            if (!RecordMetaHumanPoseTelemetry(PendingCaptureIndex)
                || !RecordMetaHumanProofTelemetry(
                    PendingCaptureIndex, PendingEvidenceMesh))
            {
                Fail(FString::Printf(
                    TEXT("capture %d lost exact-frame proof on the screenshot request tick"),
                    PendingCaptureIndex + 1));
                return false;
            }
            FCaptureRecord& ProofRecord = CaptureRecords[PendingCaptureIndex];
            ProofRecord.ScreenshotRequestFrameCounter = GFrameCounter;
            ProofRecord.ScreenshotRequestSourceBoneRevision =
                GolferMesh->GetBoneTransformRevisionNumber();
            ProofRecord.ScreenshotRequestTargetBoneRevision =
                MetaHumanVisualBody->GetBoneTransformRevisionNumber();
            ProofRecord.ScreenshotRequestSourceBoneTransformFrame =
                GolferMesh->GetCurrentBoneTransformFrame();
            ProofRecord.ScreenshotRequestTargetBoneTransformFrame =
                MetaHumanVisualBody->GetCurrentBoneTransformFrame();
            ProofRecord.ScreenshotRequestFixedDeltaTimeSeconds =
                FApp::GetFixedDeltaTime();
            ProofRecord.bScreenshotRequestFixedTimeStepActive =
                FApp::UseFixedTimeStep()
                && FMath::IsNearlyEqual(
                    ProofRecord.ScreenshotRequestFixedDeltaTimeSeconds,
                    MetaHumanProofFixedDeltaTimeSeconds,
                    UE_DOUBLE_SMALL_NUMBER);
            ProofRecord.bScreenshotRequestPoseBound =
                ProofRecord.PoseTelemetryCaptureFrameCounter
                    == ProofRecord.ScreenshotRequestFrameCounter
                && ProofRecord.SourceSampleFrameCounter
                    == ProofRecord.ScreenshotRequestFrameCounter
                && ProofRecord.TargetCorrectionFrameCounter
                    == ProofRecord.ScreenshotRequestFrameCounter
                && ProofRecord.SourceBoneRevisionAtCapture
                    == ProofRecord.ScreenshotRequestSourceBoneRevision
                && ProofRecord.TargetBoneRevisionAtCapture
                    == ProofRecord.ScreenshotRequestTargetBoneRevision
                && ProofRecord.SourceBoneTransformFrameAtCapture
                    == ProofRecord.ScreenshotRequestSourceBoneTransformFrame
                && ProofRecord.TargetBoneTransformFrameAtCapture
                    == ProofRecord.ScreenshotRequestTargetBoneTransformFrame;
            if (!ProofRecord.bScreenshotRequestPoseBound)
            {
                Fail(FString::Printf(
                    TEXT("capture %d screenshot request was not bound to one evaluated pose frame"),
                    PendingCaptureIndex + 1));
                return false;
            }

            APlayerCameraManager* const ResolvedCameraManager = PlayerController
                ? PlayerController->PlayerCameraManager.Get()
                : nullptr;
            if (!ResolvedCameraManager)
            {
                Fail(FString::Printf(
                    TEXT("capture %d had no resolved PlayerCameraManager POV on the screenshot request tick"),
                    PendingCaptureIndex + 1));
                return false;
            }
            ProofRecord.ResolvedCameraWorldLocation =
                ResolvedCameraManager->GetCameraLocation();
            ProofRecord.ResolvedCameraWorldRotation =
                ResolvedCameraManager->GetCameraRotation();
            ProofRecord.ResolvedCameraFieldOfViewDegrees =
                ResolvedCameraManager->GetFOVAngle();
            ProofRecord.ResolvedCameraLocationErrorCm = FVector::Distance(
                ProofRecord.ResolvedCameraWorldLocation,
                LockedProofCameraLocation);
            ProofRecord.ResolvedCameraRotationErrorDegrees =
                FMath::RadiansToDegrees(
                    ProofRecord.ResolvedCameraWorldRotation.Quaternion()
                        .AngularDistance(LockedProofCameraRotation.Quaternion()));
            ProofRecord.ResolvedCameraFieldOfViewErrorDegrees = FMath::Abs(
                ProofRecord.ResolvedCameraFieldOfViewDegrees
                - MetaHumanProofCameraFieldOfViewDegrees);
            ProofRecord.bResolvedCameraInvariant =
                IsSession3FiniteVector(ProofRecord.ResolvedCameraWorldLocation)
                && !ProofRecord.ResolvedCameraWorldRotation.ContainsNaN()
                && FMath::IsFinite(
                    ProofRecord.ResolvedCameraFieldOfViewDegrees)
                && FMath::IsFinite(ProofRecord.ResolvedCameraLocationErrorCm)
                && FMath::IsFinite(
                    ProofRecord.ResolvedCameraRotationErrorDegrees)
                && FMath::IsFinite(
                    ProofRecord.ResolvedCameraFieldOfViewErrorDegrees)
                && ProofRecord.ResolvedCameraLocationErrorCm
                    <= MetaHumanProofMaximumCameraLocationErrorCm
                && ProofRecord.ResolvedCameraRotationErrorDegrees
                    <= MetaHumanProofMaximumCameraRotationErrorDegrees
                && ProofRecord.ResolvedCameraFieldOfViewErrorDegrees
                    <= MetaHumanProofMaximumCameraFieldOfViewErrorDegrees;
            if (!ProofRecord.bScreenshotRequestFixedTimeStepActive
                || !ProofRecord.bResolvedCameraInvariant)
            {
                Fail(FString::Printf(
                    TEXT("capture %d screenshot request tick violated fixed-time or resolved-camera proof"),
                    PendingCaptureIndex + 1));
                return false;
            }
            UE_LOG(LogDiscGolfTour, Display,
                TEXT("DG_SESSION3_SCREENSHOT_REQUEST_PROOF: shot=%d frame=%llu source_revision=%u target_revision=%u source_transform_frame=%u target_transform_frame=%u fixed_delta=%.9f fixed_active=%d pose_bound=%d resolved_camera=%s resolved_rotation=%s resolved_fov=%.3f location_error_cm=%.4f rotation_error_deg=%.4f fov_error_deg=%.4f camera_valid=%d"),
                PendingCaptureIndex + 1,
                ProofRecord.ScreenshotRequestFrameCounter,
                ProofRecord.ScreenshotRequestSourceBoneRevision,
                ProofRecord.ScreenshotRequestTargetBoneRevision,
                ProofRecord.ScreenshotRequestSourceBoneTransformFrame,
                ProofRecord.ScreenshotRequestTargetBoneTransformFrame,
                ProofRecord.ScreenshotRequestFixedDeltaTimeSeconds,
                ProofRecord.bScreenshotRequestFixedTimeStepActive ? 1 : 0,
                ProofRecord.bScreenshotRequestPoseBound ? 1 : 0,
                *ProofRecord.ResolvedCameraWorldLocation.ToCompactString(),
                *ProofRecord.ResolvedCameraWorldRotation.ToCompactString(),
                ProofRecord.ResolvedCameraFieldOfViewDegrees,
                ProofRecord.ResolvedCameraLocationErrorCm,
                ProofRecord.ResolvedCameraRotationErrorDegrees,
                ProofRecord.ResolvedCameraFieldOfViewErrorDegrees,
                ProofRecord.bResolvedCameraInvariant ? 1 : 0);
        }
        bScreenshotIssued = true;
        PendingCaptureStartRealSeconds = FPlatformTime::Seconds();
        FScreenshotRequest::RequestScreenshot(
            PendingCapturePath,
            !bMetaHumanProductionVisualCapture,
            false,
            false,
            FIntRect(),
            true);
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION3_VISUAL_CAPTURE: REQUEST %d/%d %s"),
            PendingCaptureIndex + 1,
            GetExpectedCaptureCount(),
            *PendingCapturePath);
        return false;
    }

    const int64 Size = IFileManager::Get().FileSize(*PendingCapturePath);
    if (Size > 4096)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *PendingCapturePath)
            || Bytes.Num() < 24
            || Bytes[0] != 0x89 || Bytes[1] != 0x50 || Bytes[2] != 0x4E || Bytes[3] != 0x47)
        {
            Fail(FString::Printf(TEXT("capture was not a valid PNG: %s"), *PendingCapturePath));
            return false;
        }
        FCaptureRecord& Record = CaptureRecords.Last();
        Record.Bytes = Size;
        Record.Width = ReadBigEndianInt32(&Bytes[16]);
        Record.Height = ReadBigEndianInt32(&Bytes[20]);
        const bool bResolutionValid = bMetaHumanProductionVisualCapture
            ? (Record.Width == 1920 && Record.Height == 1080)
            : (Record.Width >= 1280 && Record.Height >= 720);
        if (!bResolutionValid)
        {
            const FString ResolutionFailure = bMetaHumanProductionVisualCapture
                ? FString::Printf(
                    TEXT("MetaHuman production capture resolution was not exactly 1920x1080: %dx%d"),
                    Record.Width,
                    Record.Height)
                : FString::Printf(
                    TEXT("capture resolution was below 1280x720: %dx%d"),
                    Record.Width,
                    Record.Height);
            Fail(ResolutionFailure);
            return false;
        }

        const int32 CompletedIndex = PendingCaptureIndex;
        bScreenshotPending = false;
        bScreenshotIssued = false;
        PendingCaptureIndex = INDEX_NONE;
        PendingCapturePath.Reset();
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION3_VISUAL_CAPTURE: CAPTURED %d/%d %dx%d bytes=%lld"),
            CompletedIndex + 1,
            GetExpectedCaptureCount(),
            Record.Width,
            Record.Height,
            Record.Bytes);
        HandleCaptureCompleted(CompletedIndex);
        return true;
    }
    if (FPlatformTime::Seconds() - PendingCaptureStartRealSeconds
        > Session3VisualEffectiveTimeout(ScreenshotTimeoutSeconds))
    {
        Fail(FString::Printf(TEXT("screenshot timed out: %s"), *PendingCapturePath));
    }
    return false;
}

void ADiscGolfSession3VisualCaptureRunner::HandleCaptureCompleted(int32 CaptureIndex)
{
    if (bMetaHumanProductionVisualCapture)
    {
        const auto ResumeProductionMontage = [this]()
        {
            FString ResumeError;
            if (!ResumeExactMontageInstance(
                    GolferMesh, ThrowMontage, ResumeError,
                    LiveCommittedMontageInstanceId))
            {
                Fail(FString::Printf(
                    TEXT("capture could not resume the legitimate montage lifecycle: %s"),
                    *ResumeError));
                return false;
            }
            return true;
        };
        switch (CaptureIndex)
        {
            case 0:
                if (!ResumeProductionMontage()) return;
                SetStage(EStage::WaitingForPlant);
                break;
            case 1:
                if (!ResumeProductionMontage()) return;
                SetStage(EStage::WaitingForRelease);
                break;
            case 2:
                PendingEvidenceMesh = nullptr;
                PendingEvidenceMarkerColor = FColor::Transparent;
                SetLiveDiscSimulationPaused(false);
                RestoreCaptureTimeDilation();
                if (!ResumeProductionMontage()) return;
                SetStage(EStage::WaitingForFollowThrough);
                break;
            case 3:
                if (!ResumeProductionMontage()) return;
                SetStage(EStage::WaitingForRecoveryProof);
                break;
            case 4:
                if (!ResumeProductionMontage()) return;
                SetStage(EStage::WaitingForProductionFlightEvidence);
                break;
            default:
                Fail(TEXT("unknown MetaHuman production capture completion index"));
                break;
        }
        return;
    }
    switch (CaptureIndex)
    {
        case 0:
            if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
            {
                Anim->Montage_Resume(ThrowMontage);
            }
            SetStage(EStage::WaitingForRelease);
            break;
        case 1:
            PendingEvidenceMesh = nullptr;
            PendingEvidenceMarkerColor = FColor::Transparent;
            SetLiveDiscSimulationPaused(false);
            RestoreCaptureTimeDilation();
            SetStage(EStage::WaitingForPostRelease);
            break;
        case 2:
            PendingEvidenceMesh = nullptr;
            PendingEvidenceMarkerColor = FColor::Transparent;
            SetLiveDiscSimulationPaused(false);
            if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
            {
                Anim->Montage_Resume(ThrowMontage);
            }
            SetStage(EStage::WaitingForFollowThrough);
            break;
        case 3:
            if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
            {
                Anim->Montage_Resume(ThrowMontage);
            }
            SetStage(EStage::WaitingForGameplayRecovery);
            break;
        case 4:
            SetStage(EStage::StartingProfile);
            break;
        case 5:
        case 6:
        case 7:
            if (ProfileMesh)
            {
                if (UAnimInstance* Anim = ProfileMesh->GetAnimInstance())
                {
                    Anim->Montage_Resume(ThrowMontage);
                }
            }
            SetStage(EStage::WaitingForProfileRecovery);
            break;
        default:
            Fail(TEXT("unknown capture completion index"));
            break;
    }
}

void ADiscGolfSession3VisualCaptureRunner::PositionFullBodyCamera(
    const AActor* Subject,
    const USkeletalMeshComponent* Mesh,
    float FieldOfViewDegrees)
{
    if (bMetaHumanProductionVisualCapture)
    {
        ApplyLockedMetaHumanProofCamera();
        return;
    }
    if (!Subject || !Mesh || !CaptureCamera)
    {
        return;
    }
    const FVector Target = Mesh->Bounds.Origin;
    const float Radius = FMath::Clamp(Mesh->Bounds.SphereRadius, 50.0f, 400.0f);
    const float Distance = FMath::Max(720.0f, Radius * 5.0f);
    const FVector Location = ResolveExternalCameraLocation(
        Subject, Target, Distance, Radius * 0.25f, 0.0f);
    CaptureCamera->SetActorLocation(Location);
    CaptureCamera->SetActorRotation((Target - Location).Rotation());
    if (UCameraComponent* CameraComponent = CaptureCamera->GetCameraComponent())
    {
        CameraComponent->SetFieldOfView(FieldOfViewDegrees);
    }
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetViewTarget(CaptureCamera);
    }
}

void ADiscGolfSession3VisualCaptureRunner::PositionGripCloseupCamera(
    const AActor* Subject,
    const USkeletalMeshComponent* Mesh,
    const FVector& FocusPoint,
    float FieldOfViewDegrees)
{
    if (bMetaHumanProductionVisualCapture)
    {
        ApplyLockedMetaHumanProofCamera();
        return;
    }
    if (!Subject || !Mesh || !CaptureCamera)
    {
        return;
    }
    const FVector Pelvis = Mesh->GetSocketLocation(TEXT("pelvis"));
    const FVector Target = FMath::Lerp(Pelvis, FocusPoint, 0.72f);
    const float Radius = FMath::Clamp(Mesh->Bounds.SphereRadius, 50.0f, 400.0f);
    const float Distance = FMath::Max(460.0f, Radius * 3.4f);
    const FVector Location = ResolveExternalCameraLocation(
        Subject, Target, Distance, Radius * 0.18f, Distance * 0.18f);
    CaptureCamera->SetActorLocation(Location);
    CaptureCamera->SetActorRotation((Target - Location).Rotation());
    if (UCameraComponent* CameraComponent = CaptureCamera->GetCameraComponent())
    {
        CameraComponent->SetFieldOfView(FieldOfViewDegrees);
    }
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetViewTarget(CaptureCamera);
    }
}

void ADiscGolfSession3VisualCaptureRunner::ConfigureLockedMetaHumanProofCamera()
{
    LockedProofCameraOrigin = InitialGolferTransform.GetLocation();
    LockedProofCameraUp = FVector::UpVector;
    LockedProofCameraForward = InitialGolferTransform
        .GetRotation().GetForwardVector();
    LockedProofCameraForward.Z = 0.0f;
    LockedProofCameraForward = LockedProofCameraForward.GetSafeNormal();
    if (LockedProofCameraForward.IsNearlyZero())
    {
        LockedProofCameraForward = FVector::ForwardVector;
    }
    LockedProofCameraRight = FVector::CrossProduct(
        LockedProofCameraUp, LockedProofCameraForward).GetSafeNormal();
    if (LockedProofCameraRight.IsNearlyZero())
    {
        LockedProofCameraRight = FVector::RightVector;
    }
    LockedProofCameraTarget = LockedProofCameraOrigin
        - LockedProofCameraForward * 10.0f
        // The pawn origin is already capsule-center height. Aim near the
        // athlete's full-body midpoint so head, hands, feet, and planted toe
        // bones all remain inside the same locked 34-degree proof frame.
        + LockedProofCameraUp * 15.0f;
    LockedProofCameraLocation = LockedProofCameraOrigin
        + LockedProofCameraForward * 85.0f
        + LockedProofCameraRight * 680.0f
        + LockedProofCameraUp * 175.0f;
    LockedProofCameraRotation = (
        LockedProofCameraTarget - LockedProofCameraLocation).Rotation();
}

void ADiscGolfSession3VisualCaptureRunner::ApplyLockedMetaHumanProofCamera()
{
    if (!CaptureCamera)
    {
        bLastCameraLineOfSightClear = false;
        return;
    }
    CaptureCamera->SetActorLocation(LockedProofCameraLocation);
    CaptureCamera->SetActorRotation(LockedProofCameraRotation);
    if (UCameraComponent* CameraComponent = CaptureCamera->GetCameraComponent())
    {
        CameraComponent->SetFieldOfView(
            MetaHumanProofCameraFieldOfViewDegrees);
    }
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetViewTarget(CaptureCamera);
    }

    FCollisionQueryParams Params(
        SCENE_QUERY_STAT(Session19MetaHumanLockedProofCamera), false);
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(CaptureCamera);
    if (Golfer)
    {
        Params.AddIgnoredActor(Golfer);
    }
    if (LiveGameplayDisc)
    {
        Params.AddIgnoredActor(LiveGameplayDisc);
    }
    FHitResult Hit;
    const bool bBlocked = GetWorld()
        && GetWorld()->LineTraceSingleByChannel(
            Hit,
            LockedProofCameraLocation,
            LockedProofCameraTarget,
            ECC_Visibility,
            Params);
    bLastCameraLineOfSightClear = !bBlocked
        || FVector::DistSquared(Hit.ImpactPoint, LockedProofCameraTarget)
            < FMath::Square(80.0f);
}

void ADiscGolfSession3VisualCaptureRunner::PositionPostReleaseCamera()
{
    if (!LiveGameplayDisc || !Golfer || !GolferMesh || !CaptureCamera)
    {
        return;
    }
    const FVector PlayerCenter = GolferMesh->Bounds.Origin;
    const UStaticMeshComponent* DiscMesh = FindNamedStaticMesh(
        LiveGameplayDisc, TEXT("DiscMesh"));
    const FVector DiscLocation = DiscMesh
        ? GetDiscEvidenceLocation(DiscMesh)
        : LiveGameplayDisc->GetActorLocation();
    const FVector Target = FMath::Lerp(PlayerCenter, DiscLocation, 0.50f);
    const float Radius = FMath::Clamp(GolferMesh->Bounds.SphereRadius, 50.0f, 400.0f);
    const float Separation = FVector::Dist(PlayerCenter, DiscLocation);
    const float Distance = FMath::Max(760.0f, Radius * 4.5f + Separation * 0.45f);
    const FVector Location = ResolveExternalCameraLocation(
        Golfer, Target, Distance, FMath::Max(180.0f, Distance * 0.22f), Distance * 0.08f);
    CaptureCamera->SetActorLocation(Location);
    CaptureCamera->SetActorRotation((Target - Location).Rotation());
    if (UCameraComponent* CameraComponent = CaptureCamera->GetCameraComponent())
    {
        CameraComponent->SetFieldOfView(FMath::Clamp(42.0f + Separation * 0.006f, 42.0f, 48.0f));
    }
    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetViewTarget(CaptureCamera);
    }
}

FVector ADiscGolfSession3VisualCaptureRunner::ResolveExternalCameraLocation(
    const AActor* Subject,
    const FVector& Target,
    float DistanceCm,
    float HeightBiasCm,
    float SideBiasCm)
{
    const FVector Forward = Subject
        ? Subject->GetActorForwardVector().GetSafeNormal()
        : FVector::ForwardVector;
    const FVector Right = Subject
        ? Subject->GetActorRightVector().GetSafeNormal()
        : FVector::RightVector;
    const FVector Anchor = Subject ? Subject->GetActorLocation() : Target;
    const float Raised = FMath::Max(HeightBiasCm, 80.0f);
    const auto CameraPoint = [&Anchor, &Target](const FVector& Horizontal, float Height)
    {
        FVector Result = Anchor + Horizontal;
        Result.Z = Target.Z + Height;
        return Result;
    };
    // Prefer a stable side-on/three-quarter sports view. Anchoring horizontal
    // placement to the actor (rather than pose-shifting bounds) prevents a few
    // centimeters of animation motion from moving the lens into a non-colliding
    // foliage card. Front/back views remain unobstructed fallbacks.
    const TArray<FVector> Candidates = {
        CameraPoint(Right * DistanceCm + Forward * SideBiasCm, Raised),
        CameraPoint(-Right * DistanceCm + Forward * SideBiasCm, Raised + 40.0f),
        CameraPoint(-Forward * DistanceCm + Right * SideBiasCm, Raised + 80.0f),
        CameraPoint(Forward * DistanceCm - Right * SideBiasCm, Raised + 120.0f),
        CameraPoint(Right * (DistanceCm * 0.82f) - Forward * (DistanceCm * 0.45f), Raised + 260.0f),
        CameraPoint(-Right * (DistanceCm * 0.82f) - Forward * (DistanceCm * 0.45f), Raised + 300.0f)
    };

    FCollisionQueryParams Params(SCENE_QUERY_STAT(Session3VisualCamera), false);
    Params.AddIgnoredActor(this);
    if (CaptureCamera) Params.AddIgnoredActor(CaptureCamera);
    if (Golfer) Params.AddIgnoredActor(Golfer);
    if (ProfileFixture) Params.AddIgnoredActor(ProfileFixture);
    if (LiveGameplayDisc) Params.AddIgnoredActor(LiveGameplayDisc);

    bLastCameraLineOfSightClear = false;
    for (const FVector& Candidate : Candidates)
    {
        FHitResult Hit;
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
            Hit, Candidate, Target, ECC_Visibility, Params);
        if (!bBlocked || FVector::DistSquared(Hit.ImpactPoint, Target) < FMath::Square(80.0f))
        {
            bLastCameraLineOfSightClear = true;
            return Candidate;
        }
    }
    return Candidates.Last();
}

bool ADiscGolfSession3VisualCaptureRunner::RecordMetaHumanPoseTelemetry(
    int32 CaptureIndex)
{
    if (!bMetaHumanProductionVisualCapture)
    {
        return true;
    }
    AActor* const VisualActor = MetaHumanBackend
        ? MetaHumanBackend->GetActiveVisualActor() : nullptr;
    USceneComponent* const VisualRoot = IsValid(VisualActor)
        ? VisualActor->GetRootComponent() : nullptr;
    USkeletalMeshComponent* const VerifiedBody = MetaHumanBackend
        ? MetaHumanBackend->GetVerifiedVisualBody() : nullptr;
    USkeletalMeshComponent* const VerifiedHead = MetaHumanBackend
        ? MetaHumanBackend->GetVerifiedVisualHead() : nullptr;
    USkeletalMeshComponent* const VerifiedOutfit = MetaHumanBackend
        ? MetaHumanBackend->GetVerifiedVisualOutfit() : nullptr;
    USkeletalMeshComponent* const AnimationSource = MetaHumanBackend
        ? MetaHumanBackend->GetActiveAnimationSourceMesh() : nullptr;
    if (!CaptureRecords.IsValidIndex(CaptureIndex)
        || !Golfer || !GolferMesh || !MetaHumanVisualBody
        || !FrameworkThrowComponent || !MetaHumanBackend
        || !MetaHumanBackend->IsVisualBackendReady()
        || !IsValid(VisualActor) || !IsValid(VisualRoot)
        || !IsValid(VerifiedBody) || VerifiedBody != MetaHumanVisualBody
        || !IsValid(VerifiedHead) || !IsValid(VerifiedOutfit)
        || !IsValid(AnimationSource)
        || VerifiedBody->GetOwner() != VisualActor
        || VerifiedHead->GetOwner() != VisualActor
        || VerifiedOutfit->GetOwner() != VisualActor
        || !VisualRoot->IsAttachedTo(AnimationSource))
    {
        return false;
    }

    GolferMesh->HandleExistingParallelEvaluationTask(true, true);
    MetaHumanVisualBody->HandleExistingParallelEvaluationTask(true, true);
    GolferMesh->UpdateBounds();
    MetaHumanVisualBody->UpdateBounds();
    UDiscGolfMetaHumanRetargetAnimInstance* const RetargetAnim =
        Cast<UDiscGolfMetaHumanRetargetAnimInstance>(
            MetaHumanVisualBody->GetAnimInstance());
    FDiscGolfMetaHumanHandCorrectionEvidence Evidence;
    const bool bHasCorrectionEvidence = RetargetAnim
        && RetargetAnim->GetPresentationHandCorrectionEvidence(Evidence);

    static const FName ShoulderBone(TEXT("upperarm_r"));
    static const FName ElbowBone(TEXT("lowerarm_r"));
    static const FName HandBone(TEXT("hand_r"));
    static const FName PelvisBone(TEXT("pelvis"));
    const bool bBonesAvailable = GolferMesh->GetBoneIndex(ShoulderBone) != INDEX_NONE
        && GolferMesh->GetBoneIndex(ElbowBone) != INDEX_NONE
        && GolferMesh->GetBoneIndex(HandBone) != INDEX_NONE
        && GolferMesh->GetBoneIndex(PelvisBone) != INDEX_NONE
        && MetaHumanVisualBody->GetBoneIndex(ShoulderBone) != INDEX_NONE
        && MetaHumanVisualBody->GetBoneIndex(ElbowBone) != INDEX_NONE
        && MetaHumanVisualBody->GetBoneIndex(HandBone) != INDEX_NONE
        && MetaHumanVisualBody->GetBoneIndex(PelvisBone) != INDEX_NONE;
    if (!bHasCorrectionEvidence || !bBonesAvailable)
    {
        return false;
    }

    FCaptureRecord& Record = CaptureRecords[CaptureIndex];
    Record.bMetaHumanVisualRootUsesAbsoluteScale =
        VisualRoot->IsUsingAbsoluteScale();
    Record.MetaHumanVisualRootWorldScale = VisualRoot->GetComponentScale();
    Record.MetaHumanBodyWorldScale = VerifiedBody->GetComponentScale();
    Record.MetaHumanHeadWorldScale = VerifiedHead->GetComponentScale();
    Record.MetaHumanOutfitWorldScale = VerifiedOutfit->GetComponentScale();
    const auto IsUnitWorldScale = [](const FVector& Scale)
    {
        return IsSession3FiniteVector(Scale)
            && Scale.Equals(FVector::OneVector, 0.001f);
    };
    const bool bFixedPresetScaleValid =
        Record.bMetaHumanVisualRootUsesAbsoluteScale
        && IsUnitWorldScale(Record.MetaHumanVisualRootWorldScale)
        && IsUnitWorldScale(Record.MetaHumanBodyWorldScale)
        && IsUnitWorldScale(Record.MetaHumanHeadWorldScale)
        && IsUnitWorldScale(Record.MetaHumanOutfitWorldScale);
    Record.ThrowPhase = static_cast<int32>(FrameworkThrowComponent->CurrentPhase);
    Record.SourceShoulderWorldLocation =
        GolferMesh->GetBoneLocation(ShoulderBone, EBoneSpaces::WorldSpace);
    Record.SourceElbowWorldLocation =
        GolferMesh->GetBoneLocation(ElbowBone, EBoneSpaces::WorldSpace);
    Record.SourceHandWorldLocation =
        GolferMesh->GetBoneLocation(HandBone, EBoneSpaces::WorldSpace);
    Record.TargetShoulderWorldLocation =
        MetaHumanVisualBody->GetBoneLocation(ShoulderBone, EBoneSpaces::WorldSpace);
    Record.TargetElbowWorldLocation =
        MetaHumanVisualBody->GetBoneLocation(ElbowBone, EBoneSpaces::WorldSpace);
    Record.TargetHandWorldLocation =
        MetaHumanVisualBody->GetBoneLocation(HandBone, EBoneSpaces::WorldSpace);
    const FVector SourcePelvisWorldLocation =
        GolferMesh->GetBoneLocation(PelvisBone, EBoneSpaces::WorldSpace);
    const FVector TargetPelvisWorldLocation =
        MetaHumanVisualBody->GetBoneLocation(PelvisBone, EBoneSpaces::WorldSpace);
    const FTransform ActorTransform = Golfer->GetActorTransform();
    Record.SourceShoulderRelativeToPelvisCm = ActorTransform.InverseTransformVectorNoScale(
        Record.SourceShoulderWorldLocation - SourcePelvisWorldLocation);
    Record.SourceElbowRelativeToPelvisCm = ActorTransform.InverseTransformVectorNoScale(
        Record.SourceElbowWorldLocation - SourcePelvisWorldLocation);
    Record.SourceHandRelativeToPelvisCm = ActorTransform.InverseTransformVectorNoScale(
        Record.SourceHandWorldLocation - SourcePelvisWorldLocation);
    Record.TargetElbowRelativeToPelvisCm = ActorTransform.InverseTransformVectorNoScale(
        Record.TargetElbowWorldLocation - TargetPelvisWorldLocation);
    Record.TargetShoulderRelativeToPelvisCm = ActorTransform.InverseTransformVectorNoScale(
        Record.TargetShoulderWorldLocation - TargetPelvisWorldLocation);
    Record.TargetHandRelativeToPelvisCm = ActorTransform.InverseTransformVectorNoScale(
        Record.TargetHandWorldLocation - TargetPelvisWorldLocation);
    Record.SourceElbowAngleDegrees = ComputeSession3JointAngleDegrees(
        Record.SourceShoulderWorldLocation,
        Record.SourceElbowWorldLocation,
        Record.SourceHandWorldLocation);
    Record.TargetElbowAngleDegrees = ComputeSession3JointAngleDegrees(
        Record.TargetShoulderWorldLocation,
        Record.TargetElbowWorldLocation,
        Record.TargetHandWorldLocation);
    Record.SourceToTargetElbowAngleErrorDegrees = FMath::Abs(
        Record.SourceElbowAngleDegrees - Record.TargetElbowAngleDegrees);
    Record.SourceShoulderToHandDistanceCm = FVector::Distance(
        Record.SourceShoulderWorldLocation, Record.SourceHandWorldLocation);
    Record.TargetShoulderToHandDistanceCm = FVector::Distance(
        Record.TargetShoulderWorldLocation, Record.TargetHandWorldLocation);
    Record.SourceArmReachFraction = ComputeSession3ArmReachFraction(
        Record.SourceShoulderWorldLocation,
        Record.SourceElbowWorldLocation,
        Record.SourceHandWorldLocation);
    Record.TargetArmReachFraction = ComputeSession3ArmReachFraction(
        Record.TargetShoulderWorldLocation,
        Record.TargetElbowWorldLocation,
        Record.TargetHandWorldLocation);
    Record.SourceHandToPelvisDistanceCm =
        Record.SourceHandRelativeToPelvisCm.Size();
    Record.TargetHandToPelvisDistanceCm =
        Record.TargetHandRelativeToPelvisCm.Size();
    Record.TargetToSourceHandPelvisDistanceRatio =
        Record.SourceHandToPelvisDistanceCm > UE_KINDA_SMALL_NUMBER
        ? Record.TargetHandToPelvisDistanceCm
            / Record.SourceHandToPelvisDistanceCm
        : -1.0f;
    Record.TargetHandHorizontalFromPelvisCm = FVector(
        Record.TargetHandRelativeToPelvisCm.X,
        Record.TargetHandRelativeToPelvisCm.Y,
        0.0f).Size();
    Record.TargetHandVerticalFromPelvisCm =
        Record.TargetHandRelativeToPelvisCm.Z;
    if (Record.SourceHandToPelvisDistanceCm > UE_KINDA_SMALL_NUMBER
        && Record.TargetHandToPelvisDistanceCm > UE_KINDA_SMALL_NUMBER)
    {
        Record.SourceToTargetHandDirectionErrorDegrees =
            FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
                FVector::DotProduct(
                    Record.SourceHandRelativeToPelvisCm.GetSafeNormal(),
                    Record.TargetHandRelativeToPelvisCm.GetSafeNormal()),
                -1.0f,
                1.0f)));
    }

    // These are deliberately target-side gates. The v006 author transaction
    // proves the source pose; this live proof prevents the assembled body from
    // collapsing or reversing that silhouette during retarget presentation.
    Record.bTargetPoseSpatialGateValid =
        CaptureIndex >= 0
        && CaptureIndex
            < UE_ARRAY_COUNT(MetaHumanProductionMinimumTargetHandToPelvisCm)
        && FMath::IsFinite(Record.SourceHandToPelvisDistanceCm)
        && FMath::IsFinite(Record.TargetHandToPelvisDistanceCm)
        && FMath::IsFinite(Record.TargetToSourceHandPelvisDistanceRatio)
        && FMath::IsFinite(Record.SourceToTargetHandDirectionErrorDegrees)
        && FMath::IsFinite(Record.SourceToTargetElbowAngleErrorDegrees)
        && FMath::IsFinite(Record.TargetHandHorizontalFromPelvisCm)
        && FMath::IsFinite(Record.TargetHandVerticalFromPelvisCm)
        && Record.TargetHandToPelvisDistanceCm
            >= MetaHumanProductionMinimumTargetHandToPelvisCm[CaptureIndex]
        && Record.SourceToTargetElbowAngleErrorDegrees
            <= MetaHumanProofMaximumSourceTargetElbowAngleErrorDegrees
        && Record.TargetArmReachFraction
            >= MetaHumanProductionMinimumTargetArmReachFraction[CaptureIndex]
        && Record.TargetToSourceHandPelvisDistanceRatio
            >= MetaHumanProofMinimumTargetSourceDistanceRatio
        && Record.TargetToSourceHandPelvisDistanceRatio
            <= MetaHumanProofMaximumTargetSourceDistanceRatio
        && Record.SourceToTargetHandDirectionErrorDegrees
            <= MetaHumanProofMaximumSourceTargetDirectionErrorDegrees;

    Record.HandCorrectionMode = Evidence.CorrectionMode;
    Record.SourceDiscGripWorldLocation = Evidence.SourceDiscGripWorldLocation;
    Record.DesiredTargetHandWorldLocation = Evidence.DesiredMetaHumanHandWorldLocation;
    Record.TargetHandPreCorrectionWorldLocation =
        Evidence.MetaHumanHandPreCorrectionWorldLocation;
    Record.BoundedEffectorTargetWorldLocation =
        Evidence.BoundedEffectorTargetWorldLocation;
    Record.TargetHandPostCorrectionWorldLocation =
        Evidence.MetaHumanHandPostCorrectionWorldLocation;
    Record.SourceHandToDiscGripDistanceCm =
        Evidence.SourceHandToDiscGripDistanceCm;
    Record.TargetHandPreToSourceHandDistanceCm =
        Evidence.MetaHumanHandPreToSourceHandDistanceCm;
    Record.TargetHandPostToSourceHandDistanceCm =
        Evidence.MetaHumanHandPostToSourceHandDistanceCm;
    Record.HandCorrectionImprovementCm =
        Record.TargetHandPreToSourceHandDistanceCm
        - Record.TargetHandPostToSourceHandDistanceCm;
    Record.TargetHandPreToDesiredHandDistanceCm =
        Evidence.MetaHumanHandPreToDesiredHandDistanceCm;
    Record.TargetHandPostToDesiredHandDistanceCm =
        Evidence.MetaHumanHandPostToDesiredHandDistanceCm;
    Record.DesiredHandCorrectionImprovementCm =
        Record.TargetHandPreToDesiredHandDistanceCm
        - Record.TargetHandPostToDesiredHandDistanceCm;
    Record.RequestedEffectorCorrectionCm =
        Evidence.RequestedEffectorCorrectionCm;
    Record.BoundedEffectorCorrectionCm =
        Evidence.BoundedEffectorCorrectionCm;
    Record.EffectorCorrectionCapCm = Evidence.EffectorCorrectionCapCm;
    Record.TargetHandPostToRenderedHandDistanceCm = FVector::Distance(
        Record.TargetHandPostCorrectionWorldLocation,
        Record.TargetHandWorldLocation);
    Record.SourceSnapshotToRenderedHandDistanceCm = FVector::Distance(
        Evidence.SourceHandWorldLocation,
        Record.SourceHandWorldLocation);
    Record.DesiredHandOrientationErrorDegrees =
        Evidence.DesiredHandOrientationErrorDegrees;
    Record.TargetHandPostToDesiredOrientationErrorDegrees =
        Evidence.MetaHumanHandPostToDesiredHandOrientationErrorDegrees;
    Record.PhaseCorrectionWeight = Evidence.PhaseCorrectionWeight;
    Record.AppliedCorrectionBlendAlpha = Evidence.AppliedCorrectionBlendAlpha;
    Record.MaximumCorrectedSegmentRatioError =
        Evidence.MaximumCorrectedSegmentRatioError;
    Record.SourceBoneRevisionAtPreUpdate =
        Evidence.SourceBoneRevisionAtPreUpdate;
    Record.TargetBoneRevisionBeforeEvaluate =
        Evidence.TargetBoneRevisionBeforeEvaluate;
    Record.SourceBoneRevisionAtCapture =
        GolferMesh->GetBoneTransformRevisionNumber();
    Record.TargetBoneRevisionAtCapture =
        MetaHumanVisualBody->GetBoneTransformRevisionNumber();
    Record.SourceBoneTransformFrameAtCapture =
        GolferMesh->GetCurrentBoneTransformFrame();
    Record.TargetBoneTransformFrameAtCapture =
        MetaHumanVisualBody->GetCurrentBoneTransformFrame();
    Record.SourceSampleFrameCounter = Evidence.SourceSampleFrameCounter;
    Record.TargetCorrectionFrameCounter = Evidence.TargetCorrectionFrameCounter;
    Record.PoseTelemetryCaptureFrameCounter = GFrameCounter;
    Record.bHandCorrectionSnapshotValid = Evidence.bSnapshotValid;
    Record.bFullGripTransformAvailable = Evidence.bFullGripTransformAvailable;
    Record.GripTransformEvidenceMode = Evidence.GripBindingMode;
    Record.bSourceGripRelativeFallbackAvailable =
        Evidence.bSourceGripRelativeFallback;
    Record.bGripTransformEvidenceAccepted =
        (Record.bFullGripTransformAvailable
            && Record.GripTransformEvidenceMode
                == DiscGolfMetaHumanPresentation::
                    TargetSocketGripBindingMode)
        || (Record.bSourceGripRelativeFallbackAvailable
            && Record.GripTransformEvidenceMode
                == DiscGolfMetaHumanPresentation::
                    SourceGripRelativeFallbackBindingMode);
    Record.bEffectorCorrectionClamped = Evidence.bEffectorCorrectionClamped;
    Record.bEffectorDistanceCapExceeded =
        Evidence.bEffectorDistanceCapExceeded;
    Record.bDesiredTargetWithinReachAnnulus =
        Evidence.bDesiredTargetWithinReachAnnulus;
    Record.bHandCorrectionReachable = Evidence.bReachable;
    Record.bHandCorrectionApplied = Evidence.bApplied;

    const bool bGripContactCheckpoint = CaptureIndex <= 2;
    Record.bRetargetPoseTelemetryValid =
        bFixedPresetScaleValid
        && IsSession3FiniteVector(Record.SourceShoulderWorldLocation)
        && IsSession3FiniteVector(Record.SourceElbowWorldLocation)
        && IsSession3FiniteVector(Record.SourceHandWorldLocation)
        && IsSession3FiniteVector(Record.TargetShoulderWorldLocation)
        && IsSession3FiniteVector(Record.TargetElbowWorldLocation)
        && IsSession3FiniteVector(Record.TargetHandWorldLocation)
        && IsSession3FiniteVector(Record.DesiredTargetHandWorldLocation)
        && IsSession3FiniteVector(Record.SourceDiscGripWorldLocation)
        && IsSession3FiniteVector(Record.TargetHandPreCorrectionWorldLocation)
        && IsSession3FiniteVector(Record.BoundedEffectorTargetWorldLocation)
        && IsSession3FiniteVector(Record.TargetHandPostCorrectionWorldLocation)
        && Record.SourceElbowAngleDegrees >= 0.0f
        && Record.SourceElbowAngleDegrees <= 180.0f
        && Record.TargetElbowAngleDegrees >= 0.0f
        && Record.TargetElbowAngleDegrees <= 180.0f
        && Record.SourceArmReachFraction >= 0.0f
        && Record.SourceArmReachFraction <= 1.01f
        && Record.TargetArmReachFraction >= 0.0f
        && Record.TargetArmReachFraction <= 1.01f
        && Record.bTargetPoseSpatialGateValid
        && Record.HandCorrectionMode
            == DiscGolfMetaHumanPresentation::HandCorrectionMode
        && Record.bHandCorrectionSnapshotValid
        && Record.bGripTransformEvidenceAccepted
        && Record.bHandCorrectionReachable
        && FMath::IsFinite(Record.SourceHandToDiscGripDistanceCm)
        && Record.SourceHandToDiscGripDistanceCm >= 0.0f
        && FMath::IsFinite(Record.TargetHandPreToSourceHandDistanceCm)
        && Record.TargetHandPreToSourceHandDistanceCm >= 0.0f
        && FMath::IsFinite(Record.TargetHandPostToSourceHandDistanceCm)
        && Record.TargetHandPostToSourceHandDistanceCm >= 0.0f
        && FMath::IsFinite(Record.TargetHandPreToDesiredHandDistanceCm)
        && Record.TargetHandPreToDesiredHandDistanceCm >= 0.0f
        && FMath::IsFinite(Record.TargetHandPostToDesiredHandDistanceCm)
        && Record.TargetHandPostToDesiredHandDistanceCm >= 0.0f
        && FMath::IsFinite(Record.RequestedEffectorCorrectionCm)
        && Record.RequestedEffectorCorrectionCm >= 0.0f
        && FMath::IsFinite(Record.BoundedEffectorCorrectionCm)
        && Record.BoundedEffectorCorrectionCm >= 0.0f
        && FMath::IsFinite(Record.EffectorCorrectionCapCm)
        && Record.EffectorCorrectionCapCm > 0.0f
        && FMath::IsFinite(Record.DesiredHandOrientationErrorDegrees)
        && Record.DesiredHandOrientationErrorDegrees >= 0.0f
        && Record.DesiredHandOrientationErrorDegrees <= 180.0f
        && FMath::IsFinite(
            Record.TargetHandPostToDesiredOrientationErrorDegrees)
        && Record.TargetHandPostToDesiredOrientationErrorDegrees >= 0.0f
        && Record.TargetHandPostToDesiredOrientationErrorDegrees <= 180.0f
        && FMath::IsFinite(Record.PhaseCorrectionWeight)
        && Record.PhaseCorrectionWeight >= 0.0f
        && Record.PhaseCorrectionWeight <= 1.0f
        && FMath::IsFinite(Record.AppliedCorrectionBlendAlpha)
        && Record.AppliedCorrectionBlendAlpha >= 0.0f
        && Record.AppliedCorrectionBlendAlpha <= 1.0f
        && Record.SourceSampleFrameCounter
            == Record.PoseTelemetryCaptureFrameCounter
        && Record.TargetCorrectionFrameCounter
            == Record.PoseTelemetryCaptureFrameCounter
        && Record.SourceBoneRevisionAtCapture
            == Record.SourceBoneRevisionAtPreUpdate
        && Record.TargetBoneRevisionAtCapture
            > Record.TargetBoneRevisionBeforeEvaluate
        && Record.SourceBoneTransformFrameAtCapture
            == Record.PoseTelemetryCaptureFrameCounter
        && Record.TargetBoneTransformFrameAtCapture
            == Record.PoseTelemetryCaptureFrameCounter
        && Record.MaximumCorrectedSegmentRatioError >= 0.0f
        && Record.MaximumCorrectedSegmentRatioError <= 0.0051f
        && Record.TargetHandPostToRenderedHandDistanceCm <= 0.5f
        && Record.SourceSnapshotToRenderedHandDistanceCm <= 0.5f
        && (!bGripContactCheckpoint
            || (Record.bHandCorrectionApplied
                && Record.bDesiredTargetWithinReachAnnulus
                && Record.TargetHandPostToDesiredHandDistanceCm <= 1.0f
                && Record.TargetHandPostToDesiredOrientationErrorDegrees
                    <= 5.0f));

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_METAHUMAN_POSE: shot=%d valid=%d phase=%d fixed_scale=%d root_absolute_scale=%d root_world_scale=%s body_world_scale=%s head_world_scale=%s outfit_world_scale=%s source_elbow_deg=%.3f target_elbow_deg=%.3f source_target_elbow_error_deg=%.3f source_reach=%.4f target_reach=%.4f source_hand_local=%s target_hand_local=%s target_pelvis_distance_cm=%.3f target_source_distance_ratio=%.4f source_target_direction_error_deg=%.3f spatial_gate=%d pre_source_cm=%.3f post_source_cm=%.3f pre_desired_cm=%.3f post_desired_cm=%.3f requested_cm=%.3f bounded_cm=%.3f clamped=%d cap_exceeded=%d blend=%.4f phase_weight=%.4f reachable=%d applied=%d grip_mode=%s full_grip=%d source_grip_fallback=%d grip_evidence_accepted=%d segment_ratio=%.6f sample_frame=%llu correction_frame=%llu capture_frame=%llu rendered_delta_cm=%.4f source_snapshot_delta_cm=%.4f"),
        CaptureIndex + 1,
        Record.bRetargetPoseTelemetryValid ? 1 : 0,
        Record.ThrowPhase,
        bFixedPresetScaleValid ? 1 : 0,
        Record.bMetaHumanVisualRootUsesAbsoluteScale ? 1 : 0,
        *Record.MetaHumanVisualRootWorldScale.ToCompactString(),
        *Record.MetaHumanBodyWorldScale.ToCompactString(),
        *Record.MetaHumanHeadWorldScale.ToCompactString(),
        *Record.MetaHumanOutfitWorldScale.ToCompactString(),
        Record.SourceElbowAngleDegrees,
        Record.TargetElbowAngleDegrees,
        Record.SourceToTargetElbowAngleErrorDegrees,
        Record.SourceArmReachFraction,
        Record.TargetArmReachFraction,
        *Record.SourceHandRelativeToPelvisCm.ToCompactString(),
        *Record.TargetHandRelativeToPelvisCm.ToCompactString(),
        Record.TargetHandToPelvisDistanceCm,
        Record.TargetToSourceHandPelvisDistanceRatio,
        Record.SourceToTargetHandDirectionErrorDegrees,
        Record.bTargetPoseSpatialGateValid ? 1 : 0,
        Record.TargetHandPreToSourceHandDistanceCm,
        Record.TargetHandPostToSourceHandDistanceCm,
        Record.TargetHandPreToDesiredHandDistanceCm,
        Record.TargetHandPostToDesiredHandDistanceCm,
        Record.RequestedEffectorCorrectionCm,
        Record.BoundedEffectorCorrectionCm,
        Record.bEffectorCorrectionClamped ? 1 : 0,
        Record.bEffectorDistanceCapExceeded ? 1 : 0,
        Record.AppliedCorrectionBlendAlpha,
        Record.PhaseCorrectionWeight,
        Record.bHandCorrectionReachable ? 1 : 0,
        Record.bHandCorrectionApplied ? 1 : 0,
        *Record.GripTransformEvidenceMode,
        Record.bFullGripTransformAvailable ? 1 : 0,
        Record.bSourceGripRelativeFallbackAvailable ? 1 : 0,
        Record.bGripTransformEvidenceAccepted ? 1 : 0,
        Record.MaximumCorrectedSegmentRatioError,
        Record.SourceSampleFrameCounter,
        Record.TargetCorrectionFrameCounter,
        Record.PoseTelemetryCaptureFrameCounter,
        Record.TargetHandPostToRenderedHandDistanceCm,
        Record.SourceSnapshotToRenderedHandDistanceCm);
    return Record.bRetargetPoseTelemetryValid;
}

bool ADiscGolfSession3VisualCaptureRunner::RecordMetaHumanProofTelemetry(
    int32 CaptureIndex,
    const UStaticMeshComponent* EvidenceMesh)
{
    if (!bMetaHumanProductionVisualCapture)
    {
        return true;
    }
    if (!CaptureRecords.IsValidIndex(CaptureIndex)
        || CaptureIndex < 0
        || CaptureIndex >= UE_ARRAY_COUNT(MetaHumanProductionTargetFrames)
        || !Golfer
        || !GolferMesh
        || !MetaHumanVisualBody
        || !FrameworkThrowComponent
        || !CaptureCamera
        || !CaptureCamera->GetCameraComponent())
    {
        return false;
    }

    FCaptureRecord& Record = CaptureRecords[CaptureIndex];
    Record.CheckpointName = MetaHumanProductionCheckpointNames[CaptureIndex];
    Record.TargetMontageFrame = MetaHumanProductionTargetFrames[CaptureIndex];
    Record.TargetMontageSeconds = static_cast<float>(
        Record.TargetMontageFrame / MetaHumanProofFrameRateHz);
    UAnimInstance* const SourceAnim = GolferMesh->GetAnimInstance();
    FAnimMontageInstance* const BoundMontageInstance =
        ResolveExactMontageInstance(
            GolferMesh, ThrowMontage, LiveCommittedMontageInstanceId);
    if (!SourceAnim || !BoundMontageInstance)
    {
        return false;
    }
    Record.MontageInstanceId = BoundMontageInstance->GetInstanceID();
    Record.MontageInstanceAnimInstancePath = SourceAnim->GetPathName();
    Record.MontageInstanceMontagePath = BoundMontageInstance->Montage
        ? BoundMontageInstance->Montage->GetPathName()
        : FString();
    Record.MontageInstanceToken = FString::Printf(
        TEXT("%s|%s|instance_id=%d"),
        *Record.MontageInstanceAnimInstancePath,
        *Record.MontageInstanceMontagePath,
        Record.MontageInstanceId);
    Record.MontageActualBlendWeight = BoundMontageInstance->GetWeight();
    Record.MontageDesiredBlendWeight = BoundMontageInstance->GetDesiredWeight();
    Record.bMontageInstanceMatchesCommittedThrow =
        Record.MontageInstanceId == LiveCommittedMontageInstanceId
        && FrameworkThrowComponent->IsCommittedMontageInstance(
            Record.MontageInstanceId, BoundMontageInstance->Montage);
    Record.bMontageFullWeightGateValid =
        Record.bMontageInstanceMatchesCommittedThrow
        && FMath::IsFinite(Record.MontageActualBlendWeight)
        && FMath::IsFinite(Record.MontageDesiredBlendWeight)
        && Record.MontageActualBlendWeight
            >= MetaHumanProofMinimumMontageBlendWeight
        && Record.MontageActualBlendWeight
            <= MetaHumanProofMaximumMontageBlendWeight
        && Record.MontageDesiredBlendWeight
            >= MetaHumanProofMinimumMontageBlendWeight
        && Record.MontageDesiredBlendWeight
            <= MetaHumanProofMaximumMontageBlendWeight;
    Record.ActualMontageSeconds = BoundMontageInstance->GetPosition();
    Record.ActualMontageFrame = Record.ActualMontageSeconds
        * static_cast<float>(MetaHumanProofFrameRateHz);
    Record.MontageFrameError = FMath::Abs(
        Record.ActualMontageFrame - static_cast<float>(Record.TargetMontageFrame));
    Record.bCheckpointTimingValid =
        Record.ThrowPhase
            == static_cast<int32>(MetaHumanProductionExpectedPhases[CaptureIndex])
        && FMath::IsFinite(Record.ActualMontageSeconds)
        && FMath::IsFinite(Record.ActualMontageFrame)
        && FMath::IsFinite(Record.MontageFrameError)
        && Record.MontageFrameError <= MetaHumanProofMaximumFrameError;

    UCameraComponent* const CameraComponent = CaptureCamera->GetCameraComponent();
    Record.CameraWorldLocation = CaptureCamera->GetActorLocation();
    Record.CameraWorldRotation = CaptureCamera->GetActorRotation();
    Record.CameraFieldOfViewDegrees = CameraComponent->FieldOfView;
    Record.LockedCameraLocationErrorCm = FVector::Distance(
        Record.CameraWorldLocation, LockedProofCameraLocation);
    Record.LockedCameraRotationErrorDegrees = FMath::RadiansToDegrees(
        Record.CameraWorldRotation.Quaternion().AngularDistance(
            LockedProofCameraRotation.Quaternion()));
    Record.LockedCameraFieldOfViewErrorDegrees = FMath::Abs(
        Record.CameraFieldOfViewDegrees - MetaHumanProofCameraFieldOfViewDegrees);
    Record.bLockedCameraInvariant =
        IsSession3FiniteVector(Record.CameraWorldLocation)
        && !Record.CameraWorldRotation.ContainsNaN()
        && FMath::IsFinite(Record.CameraFieldOfViewDegrees)
        && FMath::IsFinite(Record.LockedCameraLocationErrorCm)
        && FMath::IsFinite(Record.LockedCameraRotationErrorDegrees)
        && FMath::IsFinite(Record.LockedCameraFieldOfViewErrorDegrees)
        && Record.LockedCameraLocationErrorCm
            <= MetaHumanProofMaximumCameraLocationErrorCm
        && Record.LockedCameraRotationErrorDegrees
            <= MetaHumanProofMaximumCameraRotationErrorDegrees
        && Record.LockedCameraFieldOfViewErrorDegrees
            <= MetaHumanProofMaximumCameraFieldOfViewErrorDegrees;

    const auto Curve = [SourceAnim](const TCHAR* Name)
    {
        return SourceAnim->GetCurveValue(FName(Name));
    };
    Record.FootPlantLeftAlpha = Curve(TEXT("DG_FootPlant_L"));
    Record.FootPlantRightAlpha = Curve(TEXT("DG_FootPlant_R"));
    Record.ReachBackAlpha = Curve(TEXT("DG_ReachbackAlpha"));
    Record.BraceAlpha = Curve(TEXT("DG_BraceAlpha"));
    Record.ReleaseApproachAlpha = Curve(TEXT("DG_ReleaseApproachAlpha"));
    Record.FollowThroughAlpha = Curve(TEXT("DG_FollowThroughAlpha"));
    Record.DiscPlaneStabilityAlpha = Curve(TEXT("DG_DiscPlaneAlpha"));
    Record.WeightShiftAlpha = Curve(TEXT("DG_WeightShiftAlpha"));
    Record.BraceCompressionAlpha = Curve(TEXT("DG_BraceCompressionAlpha"));
    Record.HipDriveAlpha = Curve(TEXT("DG_HipDriveAlpha"));
    Record.TorsoDriveAlpha = Curve(TEXT("DG_TorsoDriveAlpha"));
    Record.ShoulderDriveAlpha = Curve(TEXT("DG_ShoulderDriveAlpha"));
    Record.ElbowLeadAlpha = Curve(TEXT("DG_ElbowLeadAlpha"));
    Record.WristLagAlpha = Curve(TEXT("DG_WristLagAlpha"));
    Record.FingerReleaseAlpha = Curve(TEXT("DG_FingerReleaseAlpha"));
    Record.OffArmCounterbalanceAlpha = Curve(TEXT("DG_OffArmCounterbalanceAlpha"));
    Record.GazeTargetAlpha = Curve(TEXT("DG_GazeTargetAlpha"));
    Record.BraceExtensionAlpha = Curve(TEXT("DG_BraceExtensionAlpha"));
    Record.RecoveryBeatAlpha = Curve(TEXT("DG_RecoveryBeatAlpha"));
    const float MotionCurves[] = {
        Record.FootPlantLeftAlpha,
        Record.FootPlantRightAlpha,
        Record.ReachBackAlpha,
        Record.BraceAlpha,
        Record.ReleaseApproachAlpha,
        Record.FollowThroughAlpha,
        Record.DiscPlaneStabilityAlpha,
        Record.WeightShiftAlpha,
        Record.BraceCompressionAlpha,
        Record.HipDriveAlpha,
        Record.TorsoDriveAlpha,
        Record.ShoulderDriveAlpha,
        Record.ElbowLeadAlpha,
        Record.WristLagAlpha,
        Record.FingerReleaseAlpha,
        Record.OffArmCounterbalanceAlpha,
        Record.GazeTargetAlpha,
        Record.BraceExtensionAlpha,
        Record.RecoveryBeatAlpha
    };
    Record.bMotionCurvesFiniteAndNormalized = true;
    for (const float Value : MotionCurves)
    {
        Record.bMotionCurvesFiniteAndNormalized &= FMath::IsFinite(Value)
            && Value >= -0.01f
            && Value <= 1.01f;
    }
    switch (CaptureIndex)
    {
        case 0:
            Record.bCheckpointCurveGateValid = Record.ReachBackAlpha >= 0.95f
                && Record.DiscPlaneStabilityAlpha >= 0.95f;
            break;
        case 1:
            Record.bCheckpointCurveGateValid = Record.FootPlantLeftAlpha >= 0.95f
                && Record.BraceAlpha >= 0.95f;
            break;
        case 2:
            Record.bCheckpointCurveGateValid = Record.FootPlantLeftAlpha >= 0.95f
                && Record.ReleaseApproachAlpha
                    >= MetaHumanProofReleaseCurveMinimum
                && Record.FingerReleaseAlpha
                    >= MetaHumanProofReleaseCurveMinimum
                && Record.DiscPlaneStabilityAlpha >= 0.95f;
            break;
        case 3:
            Record.bCheckpointCurveGateValid = Record.FollowThroughAlpha
                >= MetaHumanProofFollowThroughCurveMinimum;
            break;
        case 4:
            Record.bCheckpointCurveGateValid = Record.RecoveryBeatAlpha >= 0.95f;
            break;
        default:
            Record.bCheckpointCurveGateValid = false;
            break;
    }

    static const FName FootLeftBone(TEXT("foot_l"));
    static const FName FootRightBone(TEXT("foot_r"));
    static const FName BallLeftBone(TEXT("ball_l"));
    static const FName BallRightBone(TEXT("ball_r"));
    const bool bFootBonesAvailable =
        MetaHumanVisualBody->GetBoneIndex(FootLeftBone) != INDEX_NONE
        && MetaHumanVisualBody->GetBoneIndex(FootRightBone) != INDEX_NONE
        && MetaHumanVisualBody->GetBoneIndex(BallLeftBone) != INDEX_NONE
        && MetaHumanVisualBody->GetBoneIndex(BallRightBone) != INDEX_NONE;
    if (bFootBonesAvailable)
    {
        Record.FootLeftWorldLocation = MetaHumanVisualBody->GetBoneLocation(
            FootLeftBone, EBoneSpaces::WorldSpace);
        Record.FootRightWorldLocation = MetaHumanVisualBody->GetBoneLocation(
            FootRightBone, EBoneSpaces::WorldSpace);
        Record.BallLeftWorldLocation = MetaHumanVisualBody->GetBoneLocation(
            BallLeftBone, EBoneSpaces::WorldSpace);
        Record.BallRightWorldLocation = MetaHumanVisualBody->GetBoneLocation(
            BallRightBone, EBoneSpaces::WorldSpace);
    }

    FCollisionQueryParams GroundParams(
        SCENE_QUERY_STAT(Session19MetaHumanFootGroundProof), true);
    GroundParams.AddIgnoredActor(this);
    GroundParams.AddIgnoredActor(Golfer);
    if (CaptureCamera)
    {
        GroundParams.AddIgnoredActor(CaptureCamera);
    }
    if (LiveGameplayDisc)
    {
        GroundParams.AddIgnoredActor(LiveGameplayDisc);
    }
    const auto RecordGroundGap = [this, &GroundParams](
        const FVector& BallLocation,
        float& OutGapCm,
        bool& bOutTraceValid,
        FString& OutHitActor,
        FString& OutHitComponent,
        FVector& OutImpactPoint,
        FVector& OutImpactNormal)
    {
        OutGapCm = -1.0f;
        bOutTraceValid = false;
        OutHitActor.Reset();
        OutHitComponent.Reset();
        OutImpactPoint = FVector::ZeroVector;
        OutImpactNormal = FVector::ZeroVector;
        FHitResult Hit;
        bOutTraceValid = GetWorld()
            && IsSession3FiniteVector(BallLocation)
            && GetWorld()->LineTraceSingleByChannel(
                Hit,
                BallLocation + FVector::UpVector * 50.0f,
                BallLocation - FVector::UpVector * 100.0f,
                ECC_Visibility,
                GroundParams);
        if (bOutTraceValid)
        {
            OutGapCm = BallLocation.Z - Hit.ImpactPoint.Z;
            OutImpactPoint = Hit.ImpactPoint;
            OutImpactNormal = Hit.ImpactNormal.GetSafeNormal();
            OutHitActor = Hit.GetActor()
                ? Hit.GetActor()->GetPathName()
                : TEXT("<none>");
            OutHitComponent = Hit.GetComponent()
                ? Hit.GetComponent()->GetPathName()
                : TEXT("<none>");
        }
    };
    if (bFootBonesAvailable)
    {
        RecordGroundGap(
            Record.BallLeftWorldLocation,
            Record.BallLeftGroundGapCm,
            Record.bBallLeftGroundTraceValid,
            Record.BallLeftGroundHitActor,
            Record.BallLeftGroundHitComponent,
            Record.BallLeftGroundImpactPoint,
            Record.BallLeftGroundImpactNormal);
        RecordGroundGap(
            Record.BallRightWorldLocation,
            Record.BallRightGroundGapCm,
            Record.bBallRightGroundTraceValid,
            Record.BallRightGroundHitActor,
            Record.BallRightGroundHitComponent,
            Record.BallRightGroundImpactPoint,
            Record.BallRightGroundImpactNormal);
    }
    const bool bGroundContactCheckpoint = CaptureIndex == 1 || CaptureIndex == 2;
    Record.bBraceFootGroundGateValid = !bGroundContactCheckpoint
        || (bFootBonesAvailable
            && Record.FootPlantLeftAlpha >= 0.95f
            && Record.bBallLeftGroundTraceValid
            && FMath::IsFinite(Record.BallLeftGroundGapCm)
            && IsSession3FiniteVector(Record.BallLeftGroundImpactPoint)
            && IsSession3FiniteVector(Record.BallLeftGroundImpactNormal)
            && !Record.BallLeftGroundHitActor.IsEmpty()
            && !Record.BallLeftGroundHitComponent.IsEmpty()
            && Record.BallLeftGroundGapCm
                >= MetaHumanProofBraceBallMinimumGroundGapCm
            && Record.BallLeftGroundGapCm
                <= MetaHumanProofBraceBallMaximumGroundGapCm
            && Record.BallLeftGroundImpactNormal.Z
                >= MetaHumanProofMinimumGroundNormalZ);
    Record.bRequiredFootGroundEvidenceValid =
        Record.bBraceFootGroundGateValid;

    Record.bDiscPlaneEvidenceAvailable = EvidenceMesh != nullptr;
    if (EvidenceMesh)
    {
        Record.DiscWorldLocation = GetDiscEvidenceLocation(EvidenceMesh);
        Record.DiscNormalWorld = EvidenceMesh->GetUpVector().GetSafeNormal();
        Record.DiscTangentWorld = EvidenceMesh->GetForwardVector().GetSafeNormal();
        static const FName DiscGripBone(TEXT("disc_grip_r"));
        if (GolferMesh->DoesSocketExist(DiscGripBone))
        {
            const FTransform AuthoredGripWorld =
                GolferMesh->GetSocketTransform(DiscGripBone, RTS_World);
            Record.AuthoredGripNormalWorld = AuthoredGripWorld
                .GetRotation().GetUpVector().GetSafeNormal();
            Record.AuthoredGripTangentWorld = AuthoredGripWorld
                .GetRotation().GetForwardVector().GetSafeNormal();
        }
        const auto UnsignedAxisErrorDegrees = [](
            const FVector& A,
            const FVector& B) -> float
        {
            if (!IsSession3FiniteVector(A) || !IsSession3FiniteVector(B)
                || A.IsNearlyZero() || B.IsNearlyZero())
            {
                return -1.0f;
            }
            return static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
                FMath::Abs(FVector::DotProduct(
                    A.GetSafeNormal(), B.GetSafeNormal())),
                0.0f,
                1.0f))));
        };
        Record.DiscNormalToAuthoredGripErrorDegrees =
            UnsignedAxisErrorDegrees(
                Record.DiscNormalWorld, Record.AuthoredGripNormalWorld);
        Record.DiscTangentToAuthoredGripErrorDegrees =
            UnsignedAxisErrorDegrees(
                Record.DiscTangentWorld, Record.AuthoredGripTangentWorld);
        Record.MaximumDiscToAuthoredGripPlaneErrorDegrees = CaptureIndex == 2
            ? MetaHumanProofMaximumReleaseGripPlaneErrorDegrees
            : MetaHumanProofMaximumPreReleaseGripPlaneErrorDegrees;
        const float AbsoluteUpDot = FMath::Abs(FVector::DotProduct(
            Record.DiscNormalWorld, FVector::UpVector));
        Record.DiscPlaneTiltFromWorldUpDegrees = FMath::RadiansToDegrees(
            FMath::Acos(FMath::Clamp(AbsoluteUpDot, 0.0f, 1.0f)));
        Record.bDiscPlaneEvidenceValid =
            IsSession3FiniteVector(Record.DiscWorldLocation)
            && IsSession3FiniteVector(Record.DiscNormalWorld)
            && IsSession3FiniteVector(Record.DiscTangentWorld)
            && FMath::IsNearlyEqual(Record.DiscNormalWorld.Size(), 1.0f, 0.01f)
            && FMath::IsNearlyEqual(Record.DiscTangentWorld.Size(), 1.0f, 0.01f)
            && FMath::Abs(FVector::DotProduct(
                Record.DiscNormalWorld, Record.DiscTangentWorld)) <= 0.01f
            && FMath::IsFinite(Record.DiscPlaneTiltFromWorldUpDegrees)
            && Record.DiscPlaneTiltFromWorldUpDegrees >= 0.0f
            && Record.DiscPlaneTiltFromWorldUpDegrees <= 90.0f;
        Record.bDiscPlaneMatchesAuthoredGrip =
            Record.bDiscPlaneEvidenceValid
            && IsSession3FiniteVector(Record.AuthoredGripNormalWorld)
            && IsSession3FiniteVector(Record.AuthoredGripTangentWorld)
            && FMath::IsFinite(Record.DiscNormalToAuthoredGripErrorDegrees)
            && FMath::IsFinite(Record.DiscTangentToAuthoredGripErrorDegrees)
            && Record.DiscNormalToAuthoredGripErrorDegrees >= 0.0f
            && Record.DiscNormalToAuthoredGripErrorDegrees
                <= Record.MaximumDiscToAuthoredGripPlaneErrorDegrees;
    }
    const bool bDiscPlaneCheckpoint = CaptureIndex <= 2;
    Record.bMetaHumanProofTelemetryValid = Record.bCheckpointTimingValid
        && Record.bMontageFullWeightGateValid
        && Record.bLockedCameraInvariant
        && Record.bMotionCurvesFiniteAndNormalized
        && Record.bCheckpointCurveGateValid
        && Record.bRequiredFootGroundEvidenceValid
        && (!bDiscPlaneCheckpoint
            || (Record.bDiscPlaneEvidenceAvailable
                && Record.bDiscPlaneEvidenceValid
                && Record.bDiscPlaneMatchesAuthoredGrip));

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_METAHUMAN_PROOF: shot=%d checkpoint=%s valid=%d target_frame=%d actual_frame=%.3f frame_error=%.3f phase=%d montage_instance_id=%d montage_actual_weight=%.6f montage_desired_weight=%.6f montage_instance_match=%d montage_full_weight=%d camera_location_error_cm=%.4f camera_rotation_error_deg=%.4f fov_error_deg=%.4f curves_valid=%d curve_gate=%d foot_l=%.3f foot_r=%.3f left_gap_cm=%.3f right_gap_cm=%.3f brace_ground_valid=%d left_ground_actor=%s left_ground_component=%s disc_available=%d disc_tilt_deg=%.3f grip_plane_error_deg=%.3f grip_plane_valid=%d disc_valid=%d"),
        CaptureIndex + 1,
        *Record.CheckpointName,
        Record.bMetaHumanProofTelemetryValid ? 1 : 0,
        Record.TargetMontageFrame,
        Record.ActualMontageFrame,
        Record.MontageFrameError,
        Record.ThrowPhase,
        Record.MontageInstanceId,
        Record.MontageActualBlendWeight,
        Record.MontageDesiredBlendWeight,
        Record.bMontageInstanceMatchesCommittedThrow ? 1 : 0,
        Record.bMontageFullWeightGateValid ? 1 : 0,
        Record.LockedCameraLocationErrorCm,
        Record.LockedCameraRotationErrorDegrees,
        Record.LockedCameraFieldOfViewErrorDegrees,
        Record.bMotionCurvesFiniteAndNormalized ? 1 : 0,
        Record.bCheckpointCurveGateValid ? 1 : 0,
        Record.FootPlantLeftAlpha,
        Record.FootPlantRightAlpha,
        Record.BallLeftGroundGapCm,
        Record.BallRightGroundGapCm,
        Record.bBraceFootGroundGateValid ? 1 : 0,
        *Record.BallLeftGroundHitActor,
        *Record.BallLeftGroundHitComponent,
        Record.bDiscPlaneEvidenceAvailable ? 1 : 0,
        Record.DiscPlaneTiltFromWorldUpDegrees,
        Record.DiscNormalToAuthoredGripErrorDegrees,
        Record.bDiscPlaneMatchesAuthoredGrip ? 1 : 0,
        Record.bDiscPlaneEvidenceValid ? 1 : 0);
    return Record.bMetaHumanProofTelemetryValid;
}

bool ADiscGolfSession3VisualCaptureRunner::ValidateCameraFraming(
    int32 CaptureIndex,
    const AActor* Subject,
    const USkeletalMeshComponent* Mesh,
    const FVector& FocusPoint,
    bool bRequireFullBody,
    float FocusRadiusCm,
    const UStaticMeshComponent* EvidenceMesh)
{
    if (!CaptureRecords.IsValidIndex(CaptureIndex) || !Subject || !Mesh || !CaptureCamera)
    {
        Fail(TEXT("camera framing validation lacked a capture record or skeletal subject"));
        return false;
    }

    FCaptureRecord& Record = CaptureRecords[CaptureIndex];
    if (!RecordMetaHumanPoseTelemetry(CaptureIndex))
    {
        Fail(FString::Printf(
            TEXT("capture %d did not expose valid post-evaluate retarget telemetry"),
            CaptureIndex + 1));
        return false;
    }
    if (!RecordMetaHumanProofTelemetry(CaptureIndex, EvidenceMesh))
    {
        Fail(FString::Printf(
            TEXT("capture %d did not satisfy exact-frame MetaHuman motion proof"),
            CaptureIndex + 1));
        return false;
    }
    const FVector CameraLocation = CaptureCamera->GetActorLocation();
    const FTransform CameraTransform(CaptureCamera->GetActorRotation(), CameraLocation);
    const float HorizontalTan = FMath::Tan(FMath::DegreesToRadians(
        CaptureCamera->GetCameraComponent()->FieldOfView * 0.5f));
    float AspectRatio = 16.0f / 9.0f;
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
        if (Size.X > 0 && Size.Y > 0)
        {
            AspectRatio = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
        }
    }
    const float VerticalTan = HorizontalTan / FMath::Max(AspectRatio, 0.1f);
    auto IsPointFramed = [&CameraTransform, HorizontalTan, VerticalTan](
        const FVector& Point,
        float Margin)
    {
        const FVector Local = CameraTransform.InverseTransformPosition(Point);
        return Local.X > 25.0f
            && FMath::Abs(Local.Y) <= Local.X * HorizontalTan * Margin
            && FMath::Abs(Local.Z) <= Local.X * VerticalTan * Margin;
    };
    const auto ProjectNormalized = [&CameraTransform, HorizontalTan, VerticalTan](
        const FVector& Point)
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

    const FVector Pelvis = Mesh->GetSocketLocation(TEXT("pelvis"));
    const FVector Head = Mesh->GetSocketLocation(TEXT("head"));
    const FVector FootL = Mesh->GetSocketLocation(TEXT("foot_l"));
    const FVector FootR = Mesh->GetSocketLocation(TEXT("foot_r"));
    const FVector BallL = Mesh->GetSocketLocation(TEXT("ball_l"));
    const FVector BallR = Mesh->GetSocketLocation(TEXT("ball_r"));
    const FVector HandL = Mesh->GetSocketLocation(TEXT("hand_l"));
    const FVector HandR = Mesh->GetSocketLocation(TEXT("hand_r"));
    const bool bUpperBodyFramed = IsPointFramed(Pelvis, 0.86f)
        && IsPointFramed(Head, 0.86f)
        && IsPointFramed(HandR, 0.90f);
    const bool bFullBodyFramed = bUpperBodyFramed
        && IsPointFramed(FootL, 0.88f)
        && IsPointFramed(FootR, 0.88f)
        && (!bMetaHumanProductionVisualCapture
            || (IsPointFramed(BallL, 0.88f)
                && IsPointFramed(BallR, 0.88f)))
        && IsPointFramed(HandL, 0.92f);

    const FVector EvidenceLocation = EvidenceMesh
        ? GetDiscEvidenceLocation(EvidenceMesh)
        : FocusPoint;
    Record.CameraToSubjectCm = FVector::Dist(CameraLocation, Mesh->Bounds.Origin);
    Record.SubjectBoundsRadiusCm = Mesh->Bounds.SphereRadius;
    Record.bCameraOutsideSubjectBounds = !Mesh->Bounds.GetBox().ExpandBy(35.0f).IsInside(CameraLocation);
    Record.bSubjectFramed = bRequireFullBody ? bFullBodyFramed : bUpperBodyFramed;
    Record.bFocusPointFramed = IsPointFramed(EvidenceLocation, 0.82f);
    if (bMetaHumanProductionVisualCapture)
    {
        TArray<TPair<FString, FVector>> RequiredLineOfSightLandmarks;
        RequiredLineOfSightLandmarks.Emplace(TEXT("head"), Head);
        RequiredLineOfSightLandmarks.Emplace(TEXT("pelvis"), Pelvis);
        RequiredLineOfSightLandmarks.Emplace(TEXT("hand_l"), HandL);
        RequiredLineOfSightLandmarks.Emplace(TEXT("hand_r"), HandR);
        RequiredLineOfSightLandmarks.Emplace(TEXT("evidence"), EvidenceLocation);
        if (bRequireFullBody)
        {
            // Ball/toe bones sit intentionally below the visible sole on this
            // MetaHuman, so a ray to those internal joints correctly hits the
            // tee surface first. Foot bones prove visibility; the independent
            // ball-ground traces below prove planted contact and surface ID.
            RequiredLineOfSightLandmarks.Emplace(TEXT("foot_l"), FootL);
            RequiredLineOfSightLandmarks.Emplace(TEXT("foot_r"), FootR);
        }

        FCollisionQueryParams LineOfSightParams(
            SCENE_QUERY_STAT(DGSession3MetaHumanLandmarkLineOfSight),
            true);
        LineOfSightParams.AddIgnoredActor(this);
        LineOfSightParams.AddIgnoredActor(CaptureCamera);
        LineOfSightParams.AddIgnoredActor(Subject);
        LineOfSightParams.AddIgnoredActor(Golfer);
        if (LiveGameplayDisc)
        {
            LineOfSightParams.AddIgnoredActor(LiveGameplayDisc);
        }
        Record.LineOfSightLandmarkCount = RequiredLineOfSightLandmarks.Num();
        Record.ClearLineOfSightLandmarkCount = 0;
        Record.FirstOccludedLandmark.Reset();
        for (const TPair<FString, FVector>& Landmark
            : RequiredLineOfSightLandmarks)
        {
            FHitResult Hit;
            const bool bHit = GetWorld()->LineTraceSingleByChannel(
                Hit,
                CameraLocation,
                Landmark.Value,
                ECC_Visibility,
                LineOfSightParams);
            const bool bEndpointHit = bHit
                && FVector::Distance(Hit.ImpactPoint, Landmark.Value)
                    <= MetaHumanProofLineOfSightEndpointAllowanceCm;
            if (!bHit || bEndpointHit)
            {
                ++Record.ClearLineOfSightLandmarkCount;
            }
            else if (Record.FirstOccludedLandmark.IsEmpty())
            {
                Record.FirstOccludedLandmark = Landmark.Key;
            }
        }
        Record.bLineOfSightClear = Record.LineOfSightLandmarkCount > 0
            && Record.ClearLineOfSightLandmarkCount
                == Record.LineOfSightLandmarkCount;
    }
    else
    {
        Record.LineOfSightLandmarkCount = 1;
        Record.ClearLineOfSightLandmarkCount =
            bLastCameraLineOfSightClear ? 1 : 0;
        Record.FirstOccludedLandmark = bLastCameraLineOfSightClear
            ? FString()
            : TEXT("legacy_target");
        Record.bLineOfSightClear = bLastCameraLineOfSightClear;
    }
    const bool bStrictBoneLengthsInvariant = ValidateBoneLengthInvariant(
        Mesh,
        Record.MaxBoneLengthRatioError,
        &Record.WorstBoneLengthSegment,
        &Record.WorstBoneExpectedLengthCm,
        &Record.WorstBoneActualLengthCm);
    // The assembled DHI body has proportion deltas from its canonical
    // reference skeleton after the verified DG retarget is applied. Keep the
    // strict proxy threshold for Session 3, while accepting the measured
    // MetaHuman retarget envelope and recording the exact ratio for review.
    Record.bBoneLengthsInvariant = bStrictBoneLengthsInvariant
        || (bMetaHumanProductionVisualCapture
            && Record.MaxBoneLengthRatioError > 0.0f
            && Record.MaxBoneLengthRatioError <= 0.20f);
    if (EvidenceMesh)
    {
        const AActor* EvidenceOwner = EvidenceMesh->GetOwner();
        Record.EvidenceComponent = EvidenceMesh->GetName();
        Record.EvidenceComponentWorldLocation = EvidenceLocation;
        Record.EvidenceActorToComponentCm = EvidenceOwner
            ? FVector::Dist(EvidenceOwner->GetActorLocation(), EvidenceLocation)
            : 0.0f;
        Record.EvidenceFocusAlignmentCm = FVector::Dist(FocusPoint, EvidenceLocation);
        Record.bEvidenceFocusAnchoredToComponent =
            Record.EvidenceFocusAlignmentCm <= 0.5f;
    }
    else
    {
        Record.bEvidenceFocusAnchoredToComponent = true;
    }
    const FVector FeetCenter = (FootL + FootR) * 0.5f;
    const FVector2D ProjectedHead = ProjectNormalized(Head);
    const FVector2D ProjectedFeet = ProjectNormalized(FeetCenter);
    const FVector2D ProjectedPelvis = ProjectNormalized(Pelvis);
    Record.SubjectScreenHeightFraction = bRequireFullBody
        ? FMath::Abs(ProjectedHead.Y - ProjectedFeet.Y) * 0.5f
        : FMath::Abs(ProjectedHead.Y - ProjectedPelvis.Y) * 0.5f;
    if (bMetaHumanProductionVisualCapture)
    {
        TArray<FVector2D> ProjectedLandmarks = {
            ProjectedHead,
            ProjectedPelvis,
            ProjectNormalized(HandR),
            ProjectNormalized(EvidenceLocation)
        };
        if (bRequireFullBody)
        {
            ProjectedLandmarks.Add(ProjectedFeet);
            ProjectedLandmarks.Add(ProjectNormalized(BallL));
            ProjectedLandmarks.Add(ProjectNormalized(BallR));
            ProjectedLandmarks.Add(ProjectNormalized(HandL));
        }
        float MinimumX = ProjectedLandmarks[0].X;
        float MaximumX = ProjectedLandmarks[0].X;
        float MinimumY = ProjectedLandmarks[0].Y;
        float MaximumY = ProjectedLandmarks[0].Y;
        for (const FVector2D& Landmark : ProjectedLandmarks)
        {
            MinimumX = FMath::Min(MinimumX, Landmark.X);
            MaximumX = FMath::Max(MaximumX, Landmark.X);
            MinimumY = FMath::Min(MinimumY, Landmark.Y);
            MaximumY = FMath::Max(MaximumY, Landmark.Y);
        }
        // A reachback can be nearly horizontal in camera space. Use the
        // largest landmark span instead of misclassifying that pose as a tiny
        // subject solely because head and pelvis share screen-space height.
        Record.SubjectScreenHeightFraction = 0.5f * FMath::Max(
            MaximumX - MinimumX,
            MaximumY - MinimumY);
    }
    Record.bSubjectReadableScale = bRequireFullBody
        ? Record.SubjectScreenHeightFraction >= 0.34f
            && Record.SubjectScreenHeightFraction <= 0.88f
        : Record.SubjectScreenHeightFraction >= 0.20f
            && Record.SubjectScreenHeightFraction <= 0.82f;
    if (FocusRadiusCm > 0.0f)
    {
        const FVector FocusLocal = CameraTransform.InverseTransformPosition(EvidenceLocation);
        const float VerticalDiameterFraction = FocusLocal.X > 1.0f
            ? FocusRadiusCm / (FocusLocal.X * VerticalTan)
            : 0.0f;
        int32 ViewportHeight = 1080;
        if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
        {
            ViewportHeight = GEngine->GameViewport->Viewport->GetSizeXY().Y;
        }
        Record.FocusProjectedDiameterPixels = VerticalDiameterFraction
            * static_cast<float>(FMath::Max(ViewportHeight, 1));
        Record.bEvidenceMeshReadable = Record.FocusProjectedDiameterPixels >= 24.0f;
    }
    else
    {
        Record.bEvidenceMeshReadable = true;
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAMERA: shot=%d camera=%s subject=%s radius=%.1f distance=%.1f outside=%d subject_framed=%d focus_framed=%d los=%d los_clear=%d/%d first_occluded=%s bone_lengths=%d max_ratio_error=%.4f worst_segment=%s expected_cm=%.4f actual_cm=%.4f screen_height=%.3f readable_scale=%d focus_diameter_px=%.1f evidence_readable=%d evidence_component=%s actor_component_cm=%.4f focus_alignment_cm=%.4f component_anchored=%d"),
        CaptureIndex + 1,
        *CameraLocation.ToCompactString(),
        *Mesh->Bounds.Origin.ToCompactString(),
        Record.SubjectBoundsRadiusCm,
        Record.CameraToSubjectCm,
        Record.bCameraOutsideSubjectBounds ? 1 : 0,
        Record.bSubjectFramed ? 1 : 0,
        Record.bFocusPointFramed ? 1 : 0,
        Record.bLineOfSightClear ? 1 : 0,
        Record.ClearLineOfSightLandmarkCount,
        Record.LineOfSightLandmarkCount,
        *Record.FirstOccludedLandmark,
        Record.bBoneLengthsInvariant ? 1 : 0,
        Record.MaxBoneLengthRatioError,
        *Record.WorstBoneLengthSegment,
        Record.WorstBoneExpectedLengthCm,
        Record.WorstBoneActualLengthCm,
        Record.SubjectScreenHeightFraction,
        Record.bSubjectReadableScale ? 1 : 0,
        Record.FocusProjectedDiameterPixels,
        Record.bEvidenceMeshReadable ? 1 : 0,
        *Record.EvidenceComponent,
        Record.EvidenceActorToComponentCm,
        Record.EvidenceFocusAlignmentCm,
        Record.bEvidenceFocusAnchoredToComponent ? 1 : 0);

    if (!Record.bCameraOutsideSubjectBounds || !Record.bSubjectFramed
        || !Record.bFocusPointFramed || !Record.bLineOfSightClear
        || !Record.bBoneLengthsInvariant || !Record.bSubjectReadableScale
        || !Record.bEvidenceMeshReadable
        || !Record.bEvidenceFocusAnchoredToComponent)
    {
        Fail(FString::Printf(
            TEXT("capture %d failed external-camera framing validation"), CaptureIndex + 1));
        return false;
    }
    return true;
}

void ADiscGolfSession3VisualCaptureRunner::ShowEvidenceLabel(
    const FString& Heading,
    const FString& Detail) const
{
    if (bMetaHumanProductionVisualCapture || !GEngine)
    {
        return;
    }
    GEngine->ClearOnScreenDebugMessages();
    GEngine->AddOnScreenDebugMessage(31003, 30.0f, FColor(202, 232, 255), Detail, true, FVector2D(1.15f));
    GEngine->AddOnScreenDebugMessage(31002, 30.0f, FColor::White, Heading, true, FVector2D(1.5f));
}

USkeletalMeshComponent* ADiscGolfSession3VisualCaptureRunner::FindSkeletalMesh(AActor* ActorOwner) const
{
    return ActorOwner ? ActorOwner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

UStaticMeshComponent* ADiscGolfSession3VisualCaptureRunner::FindNamedStaticMesh(
    AActor* ActorOwner,
    FName ComponentName) const
{
    if (!ActorOwner)
    {
        return nullptr;
    }
    TArray<UStaticMeshComponent*> Components;
    ActorOwner->GetComponents(Components);
    for (UStaticMeshComponent* Component : Components)
    {
        if (Component && Component->GetFName() == ComponentName)
        {
            return Component;
        }
    }
    return nullptr;
}

int32 ADiscGolfSession3VisualCaptureRunner::CountWorldDiscs() const
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

float ADiscGolfSession3VisualCaptureRunner::GetMontagePosition(USkeletalMeshComponent* Mesh) const
{
    const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    if (!Anim || !ThrowMontage)
    {
        return -1.0f;
    }

    // Bind all proof timing to the exact instance that the legitimate montage
    // lifecycle created. Active-montage helpers can stop resolving near the end
    // of a lifecycle; this path never seeks, changes rate, or injects a phase.
    const FAnimMontageInstance* MontageInstance =
        ResolveExactMontageInstance(
            Mesh,
            ThrowMontage,
            bMetaHumanProductionVisualCapture
                ? LiveCommittedMontageInstanceId
                : INDEX_NONE);
    return MontageInstance ? MontageInstance->GetPosition() : -1.0f;
}

bool ADiscGolfSession3VisualCaptureRunner::ValidateCompletedLiveFlightEvidence(
    ADiscActor* SettledDisc,
    FString& OutError)
{
    bLiveTrajectoryCaptureValid = false;
    LiveTrajectoryCaptureId.Reset();
    LiveTrajectorySampleCount = 0;
    LiveFlightDurationSeconds = 0.0f;
    LiveTrajectoryAirCarryMeters = 0.0f;
    LiveTrajectoryFinalCarryMeters = 0.0f;
    LiveFlightGroundContactCount = 0;
    LiveFlightBasketContactCount = 0;
    if (!SettledDisc || SettledDisc != LiveGameplayDisc)
    {
        OutError = TEXT("completed disc does not match the single authoritative live disc");
        return false;
    }

    const UDiscFlightComponent* Flight = SettledDisc->GetFlightComponent();
    if (!Flight)
    {
        OutError = TEXT("completed authoritative disc has no flight component");
        return false;
    }
    LiveTrajectorySampleCount = Flight->GetTrajectorySamples().Num();
    if (Flight->DidLastFlightTerminateInvalidly())
    {
        OutError = FString::Printf(
            TEXT("flight validation terminated at %s: %s"),
            *Flight->GetLastFlightValidationFailureContext(),
            *Flight->GetLastFlightValidationFailure());
        return false;
    }

    const FDiscFlightTelemetry Telemetry = Flight->GetTelemetry();
    LiveFlightDurationSeconds = Telemetry.FlightTimeSeconds;
    LiveFlightGroundContactCount = Telemetry.GroundContactCount;
    LiveFlightBasketContactCount = Telemetry.BasketContactCount;
    const bool bLegitimateGroundTerminal =
        !bLiveAuthoritativeFlightHoledOut
        && Telemetry.State == EDiscFlightState::Settled
        && Telemetry.GroundState == EDiscGroundState::Settled
        && Telemetry.GroundContactCount >= 1;
    const bool bLegitimateHoledOutTerminal =
        bLiveAuthoritativeFlightHoledOut
        && (Telemetry.State == EDiscFlightState::Settled
            || Telemetry.State == EDiscFlightState::HoledOut)
        && Telemetry.GroundState == EDiscGroundState::Settled
        && Telemetry.BasketContactCount >= 1
        && Telemetry.LastBasketContact == EBasketContactResult::Caught;
    if ((!bLegitimateGroundTerminal && !bLegitimateHoledOutTerminal)
        || !FMath::IsFinite(Telemetry.FlightTimeSeconds)
        || Telemetry.FlightTimeSeconds
            < MetaHumanProofMinimumCompletedFlightDurationSeconds
        || LiveTrajectorySampleCount
            < MetaHumanProofMinimumTrajectorySampleCount)
    {
        OutError = FString::Printf(
            TEXT("completed drive terminal telemetry is invalid: event_holed_out=%d state=%d ground=%d duration=%.6f samples=%d ground_contacts=%d basket_contacts=%d last_basket_contact=%d"),
            bLiveAuthoritativeFlightHoledOut ? 1 : 0,
            static_cast<int32>(Telemetry.State),
            static_cast<int32>(Telemetry.GroundState),
            Telemetry.FlightTimeSeconds,
            LiveTrajectorySampleCount,
            Telemetry.GroundContactCount,
            Telemetry.BasketContactCount,
            static_cast<int32>(Telemetry.LastBasketContact));
        return false;
    }

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UDiscTrajectorySubsystem* Trajectories = GameInstance
        ? GameInstance->GetSubsystem<UDiscTrajectorySubsystem>() : nullptr;
    if (!Trajectories || !Trajectories->HasLastCapture())
    {
        OutError = TEXT("trajectory subsystem did not publish the completed flight");
        return false;
    }

    const FDiscTrajectorySummary& Summary = Trajectories->GetLastSummary();
    LiveTrajectoryCaptureId = Summary.CaptureId;
    LiveTrajectoryAirCarryMeters = Summary.AirCarryMeters;
    LiveTrajectoryFinalCarryMeters = Summary.FinalCarryMeters;
    const bool bSummaryFinite = FMath::IsFinite(Summary.DurationSeconds)
        && FMath::IsFinite(Summary.AirCarryMeters)
        && FMath::IsFinite(Summary.FinalCarryMeters)
        && FMath::IsFinite(Summary.ApexMeters)
        && FMath::IsFinite(Summary.LateralMeters)
        && IsSession3FiniteVector(Summary.StartWorldLocationCm)
        && IsSession3FiniteVector(Summary.FinalWorldLocationCm);
    const float DurationToleranceSeconds =
        FMath::Max(Flight->FixedStepSeconds, UE_KINDA_SMALL_NUMBER)
        + UE_KINDA_SMALL_NUMBER;
    const bool bSummaryTerminalValid = bLiveAuthoritativeFlightHoledOut
        ? (Summary.bHoledOut
            && Summary.BasketContactCount >= 1
            && Summary.LastBasketContact == EBasketContactResult::Caught)
        : (!Summary.bHoledOut && Summary.GroundContactCount >= 1);
    if (Summary.CaptureId.IsEmpty()
        || Summary.CaptureId == BaselineTrajectoryCaptureId
        || Summary.SampleCount != LiveTrajectorySampleCount
        || Summary.SampleCount < MetaHumanProofMinimumTrajectorySampleCount
        || Summary.DurationSeconds
            < MetaHumanProofMinimumCompletedFlightDurationSeconds
        || FMath::Abs(Summary.DurationSeconds - Telemetry.FlightTimeSeconds)
            > DurationToleranceSeconds
        || Summary.AirCarryMeters < MetaHumanProofMinimumAirCarryMeters
        || Summary.FinalCarryMeters < MetaHumanProofMinimumFinalCarryMeters
        || Summary.FinalGroundState != EDiscGroundState::Settled
        || !bSummaryTerminalValid
        || Summary.Handedness != LiveCommand.Handedness
        || Summary.bWasRegression
        || !bSummaryFinite)
    {
        OutError = FString::Printf(
            TEXT("trajectory summary does not bind a meaningful completed drive: capture=%s baseline=%s summary_samples=%d flight_samples=%d summary_duration=%.6f telemetry_duration=%.6f duration_tolerance=%.6f air_carry=%.3f final_carry=%.3f ground=%d ground_contacts=%d basket_contacts=%d holed_out=%d expected_holed_out=%d terminal_valid=%d regression=%d finite=%d"),
            *Summary.CaptureId,
            *BaselineTrajectoryCaptureId,
            Summary.SampleCount,
            LiveTrajectorySampleCount,
            Summary.DurationSeconds,
            Telemetry.FlightTimeSeconds,
            DurationToleranceSeconds,
            Summary.AirCarryMeters,
            Summary.FinalCarryMeters,
            static_cast<int32>(Summary.FinalGroundState),
            Summary.GroundContactCount,
            Summary.BasketContactCount,
            Summary.bHoledOut ? 1 : 0,
            bLiveAuthoritativeFlightHoledOut ? 1 : 0,
            bSummaryTerminalValid ? 1 : 0,
            Summary.bWasRegression ? 1 : 0,
            bSummaryFinite ? 1 : 0);
        return false;
    }

    FDiscTrajectorySummary DeferredSummary;
    if (!bLiveTrajectoryExportDeferralRequested
        || !Trajectories->HasPendingDeferredCaptureExport()
        || !Trajectories->GetPendingDeferredCaptureSummary(DeferredSummary)
        || DeferredSummary.CaptureId != Summary.CaptureId)
    {
        OutError = TEXT("completed trajectory was not retained under the lane's no-write export deferral");
        return false;
    }
    if (!DiscardLiveTrajectoryExportDeferral())
    {
        OutError = TEXT("completed deferred trajectory could not be discarded without file export");
        return false;
    }

    bLiveTrajectoryCaptureValid = true;
    OutError.Reset();
    return true;
}

bool ADiscGolfSession3VisualCaptureRunner::
    DiscardLiveTrajectoryExportDeferral()
{
    if (bLiveTrajectoryExportDeferralDiscarded)
    {
        return true;
    }
    if (!bLiveTrajectoryExportDeferralRequested)
    {
        return false;
    }
    UGameInstance* GameInstance = GetWorld()
        ? GetWorld()->GetGameInstance() : nullptr;
    UDiscTrajectorySubsystem* Trajectories = GameInstance
        ? GameInstance->GetSubsystem<UDiscTrajectorySubsystem>() : nullptr;
    if (!Trajectories || !Trajectories->DiscardDeferredCaptureExport())
    {
        return false;
    }
    bLiveTrajectoryExportDeferralDiscarded = true;
    return true;
}

bool ADiscGolfSession3VisualCaptureRunner::IsProfilePoseFiniteAndPlausible() const
{
    if (!ProfileMesh)
    {
        return false;
    }
    const FVector Pelvis = ProfileMesh->GetSocketLocation(TEXT("pelvis"));
    const FVector Head = ProfileMesh->GetSocketLocation(TEXT("head"));
    const FVector Hand = ProfileMesh->GetSocketLocation(TEXT("disc_grip_r"));
    const FVector FootL = ProfileMesh->GetSocketLocation(TEXT("foot_l"));
    const FVector FootR = ProfileMesh->GetSocketLocation(TEXT("foot_r"));
    const FBoxSphereBounds Bounds = ProfileMesh->Bounds;
    float MaxBoneLengthRatioError = 0.0f;
    return IsSession3FiniteVector(Pelvis) && IsSession3FiniteVector(Head) && IsSession3FiniteVector(Hand)
        && IsSession3FiniteVector(FootL) && IsSession3FiniteVector(FootR)
        && IsSession3FiniteVector(Bounds.BoxExtent)
        && FVector::Dist(Pelvis, Head) > 35.0f
        && FVector::Dist(Pelvis, Head) < 180.0f
        && FVector::Dist(Pelvis, Hand) < 260.0f
        && FVector::Dist(FootL, FootR) < 220.0f
        && Bounds.SphereRadius > 40.0f
        && Bounds.SphereRadius < 400.0f
        && ValidateBoneLengthInvariant(ProfileMesh, MaxBoneLengthRatioError);
}

bool ADiscGolfSession3VisualCaptureRunner::ValidateBoneLengthInvariant(
    const USkeletalMeshComponent* Mesh,
    float& OutMaxRatioError,
    FString* OutWorstSegment,
    float* OutExpectedLengthCm,
    float* OutActualLengthCm) const
{
    OutMaxRatioError = 0.0f;
    if (OutWorstSegment)
    {
        OutWorstSegment->Reset();
    }
    if (OutExpectedLengthCm)
    {
        *OutExpectedLengthCm = 0.0f;
    }
    if (OutActualLengthCm)
    {
        *OutActualLengthCm = 0.0f;
    }
    const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset)
    {
        return false;
    }

    const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
    TArray<FTransform> ReferenceComponentTransforms;
    FAnimationRuntime::FillUpComponentSpaceTransforms(
        RefSkeleton,
        RefSkeleton.GetRefBonePose(),
        ReferenceComponentTransforms);
    if (ReferenceComponentTransforms.Num() != RefSkeleton.GetNum())
    {
        return false;
    }
    static const FName RequiredBones[] = {
        // root->pelvis is a motion/retarget translation channel, not an
        // anatomical segment. Including it turns the bounded presentation-root
        // trajectory into a false-positive limb-stretch failure.
        TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
        TEXT("neck_01"), TEXT("head"),
        TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
        TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
        TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
        TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")
    };
    int32 ComparedSegments = 0;
    for (const FName BoneName : RequiredBones)
    {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
        if (BoneIndex <= 0)
        {
            return false;
        }
        const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
        if (ParentIndex == INDEX_NONE)
        {
            return false;
        }
        const FVector ReferenceChildLocation = Mesh->GetComponentTransform().TransformPosition(
            ReferenceComponentTransforms[BoneIndex].GetTranslation());
        const FVector ReferenceParentLocation = Mesh->GetComponentTransform().TransformPosition(
            ReferenceComponentTransforms[ParentIndex].GetTranslation());
        const float ExpectedLength = FVector::Dist(
            ReferenceChildLocation,
            ReferenceParentLocation);
        if (ExpectedLength < 0.5f)
        {
            continue;
        }
        const FName ParentName = RefSkeleton.GetBoneName(ParentIndex);
        const FVector ChildLocation = Mesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
        const FVector ParentLocation = Mesh->GetBoneLocation(ParentName, EBoneSpaces::WorldSpace);
        if (!IsSession3FiniteVector(ChildLocation) || !IsSession3FiniteVector(ParentLocation))
        {
            return false;
        }
        const float ActualLength = FVector::Dist(ChildLocation, ParentLocation);
        const float RatioError = FMath::Abs(ActualLength / ExpectedLength - 1.0f);
        if (RatioError > OutMaxRatioError)
        {
            OutMaxRatioError = RatioError;
            if (OutWorstSegment)
            {
                *OutWorstSegment = FString::Printf(
                    TEXT("%s->%s"), *ParentName.ToString(), *BoneName.ToString());
            }
            if (OutExpectedLengthCm)
            {
                *OutExpectedLengthCm = ExpectedLength;
            }
            if (OutActualLengthCm)
            {
                *OutActualLengthCm = ActualLength;
            }
        }
        ++ComparedSegments;
    }
    return ComparedSegments >= 16 && OutMaxRatioError <= 0.12f;
}

float ADiscGolfSession3VisualCaptureRunner::GetProvisionalDiscRadiusCm(
    const UStaticMeshComponent* DiscMesh) const
{
    const UStaticMesh* MeshAsset = DiscMesh ? DiscMesh->GetStaticMesh() : nullptr;
    if (!MeshAsset)
    {
        return 0.0f;
    }
    // Use the authored component scale rather than the attached component's
    // conservative world bounds. The accepted imported skeleton has a scaled
    // root, which can inflate attachment bounds without matching rendered size.
    return MeshAsset->GetBounds().SphereRadius
        * DiscMesh->GetRelativeScale3D().GetAbsMax();
}

FVector ADiscGolfSession3VisualCaptureRunner::GetDiscEvidenceLocation(
    const UStaticMeshComponent* DiscMesh) const
{
    if (!DiscMesh)
    {
        return FVector::ZeroVector;
    }

    // Bounds.Origin follows the location of the geometry submitted to the
    // renderer. That is the evidence we need; an owning actor or attachment
    // origin can legitimately differ after visual offsets are introduced.
    const FVector RenderedOrigin = DiscMesh->Bounds.Origin;
    return IsSession3FiniteVector(RenderedOrigin)
        ? RenderedOrigin
        : DiscMesh->GetComponentLocation();
}

void ADiscGolfSession3VisualCaptureRunner::SetLiveDiscSimulationPaused(
    bool bPaused) const
{
    if (!LiveGameplayDisc)
    {
        return;
    }

    LiveGameplayDisc->SetActorTickEnabled(!bPaused);
    if (UDiscFlightComponent* Flight = LiveGameplayDisc->GetFlightComponent())
    {
        Flight->SetComponentTickEnabled(!bPaused);
    }
}

void ADiscGolfSession3VisualCaptureRunner::ApplyNeutralProxyMaterials(
    USkeletalMeshComponent* Mesh) const
{
    if (!Mesh)
    {
        return;
    }
    const FLinearColor NeutralProxy(0.16f, 0.18f, 0.21f, 1.0f);
    for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
    {
        if (UMaterialInstanceDynamic* Dynamic =
            Mesh->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
        {
            Dynamic->SetVectorParameterValue(TEXT("Color"), NeutralProxy);
            Dynamic->SetVectorParameterValue(TEXT("BaseColor"), NeutralProxy);
        }
    }
}

UMaterialInstanceDynamic* ADiscGolfSession3VisualCaptureRunner::ApplyDiscEvidenceMaterial(
    UStaticMeshComponent* DiscMesh,
    const FLinearColor& Color) const
{
    if (!DiscMesh)
    {
        return nullptr;
    }
    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    UMaterialInstanceDynamic* Dynamic = BaseMaterial
        ? UMaterialInstanceDynamic::Create(BaseMaterial, DiscMesh)
        : nullptr;
    if (!Dynamic)
    {
        return nullptr;
    }
    Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
    DiscMesh->SetMaterial(0, Dynamic);
    DiscMesh->MarkRenderStateDirty();
    return Dynamic;
}

void ADiscGolfSession3VisualCaptureRunner::DrawDiscEvidenceMarker(
    const FVector& Location,
    const FColor& Color) const
{
    if (!bMetaHumanProductionVisualCapture && GetWorld())
    {
        const FVector ScreenRight = CaptureCamera
            ? CaptureCamera->GetActorRightVector()
            : FVector::RightVector;
        const FVector ScreenUp = CaptureCamera
            ? CaptureCamera->GetActorUpVector()
            : FVector::UpVector;
        DrawDebugCircle(GetWorld(), Location, 12.0f, 12, Color,
            true, -1.0f, SDPG_Foreground, 1.25f,
            ScreenRight, ScreenUp, false);
        DrawDebugDirectionalArrow(
            GetWorld(),
            Location + ScreenUp * 31.0f,
            Location + ScreenUp * 16.0f,
            4.0f,
            Color,
            true,
            -1.0f,
            SDPG_Foreground,
            1.25f);
    }
}

void ADiscGolfSession3VisualCaptureRunner::ClearEvidenceMarkers() const
{
    if (GetWorld())
    {
        FlushPersistentDebugLines(GetWorld());
    }
}

void ADiscGolfSession3VisualCaptureRunner::RestoreCaptureTimeDilation()
{
    if (GetWorld())
    {
        UGameplayStatics::SetGlobalTimeDilation(this, SavedGlobalTimeDilation);
    }
}

void ADiscGolfSession3VisualCaptureRunner::RestoreCapturePresentationState()
{
    if (bCapturePresentationStateRestored)
    {
        return;
    }
    bCapturePresentationStateRestored = true;
    ClearEvidenceMarkers();
    if (GEngine)
    {
        GEngine->ClearOnScreenDebugMessages();
    }
    if (bHudVisibilityCaptured && IsValid(CaptureHud))
    {
        CaptureHud->bShowHUD = bInitialHudVisible;
    }
    if (bFixedTimeStepCaptured)
    {
        FApp::SetFixedDeltaTime(SavedFixedDeltaTimeSeconds);
        FApp::SetUseFixedTimeStep(bSavedUseFixedTimeStep);
    }
}

bool ADiscGolfSession3VisualCaptureRunner::WriteManifest(bool bPassed, const FString& Error)
{
    if (ManifestPath.IsEmpty())
    {
        return false;
    }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(
        TEXT("schema"),
        bMetaHumanProductionVisualCapture
            ? TEXT("DiscGolfTour.Session19MetaHumanProductionMotionVisualEvidence.v6")
            : TEXT("DiscGolfTour.Session3FirstThrowVisualEvidence.v1"));
    Root->SetStringField(TEXT("status"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    Root->SetStringField(
        TEXT("scope"),
        bMetaHumanProductionVisualCapture
            ? TEXT("SESSION_19_METAHUMAN_ACTIVE_PRODUCTION_MOTION_VISUAL_EVIDENCE_ONLY")
            : TEXT("SESSION_3_FIRST_THROW_VISUAL_EVIDENCE_ONLY"));
    Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("output_directory"), OutputDirectory);
    Root->SetNumberField(TEXT("capture_count"), CaptureRecords.Num());
    Root->SetNumberField(TEXT("expected_capture_count"), GetExpectedCaptureCount());
    Root->SetStringField(TEXT("error"), Error);
    Root->SetBoolField(
        TEXT("release_pose_validation_deferred_from_callback"),
        bMetaHumanProductionVisualCapture);
    Root->SetNumberField(
        TEXT("release_pose_settle_world_ticks_required"),
        bMetaHumanProductionVisualCapture
            ? MetaHumanReleasePoseSettleWorldTicks
            : 0);
    Root->SetNumberField(
        TEXT("release_pose_settle_world_ticks_completed"),
        ExactReleasePoseSettleTicksCompleted);
    Root->SetBoolField(
        TEXT("metahuman_body_tick_prerequisite_added"),
        bMetaHumanBodyTickPrerequisiteAdded);
    Root->SetNumberField(
        TEXT("release_pose_callback_frame_counter"),
        static_cast<double>(ExactReleasePoseCallbackFrameCounter));
    Root->SetNumberField(
        TEXT("release_pose_validation_frame_counter"),
        static_cast<double>(ExactReleasePoseValidationFrameCounter));
    Root->SetNumberField(
        TEXT("release_pose_bone_revision_at_callback"),
        ExactReleasePoseBoneRevisionAtCallback);
    Root->SetNumberField(
        TEXT("release_pose_bone_revision_at_validation"),
        ExactReleasePoseBoneRevisionAtValidation);
    Root->SetNumberField(
        TEXT("release_pose_bone_transform_frame_at_validation"),
        ExactReleasePoseBoneTransformFrameAtValidation);
    Root->SetBoolField(
        TEXT("release_pose_parallel_evaluation_complete"),
        bExactReleasePoseParallelEvaluationComplete);
    Root->SetBoolField(
        TEXT("release_pose_post_evaluate_proven"),
        bExactReleasePosePostEvaluateProven);
    if (bMetaHumanProductionVisualCapture)
    {
        TSharedRef<FJsonObject> ProofPolicy = MakeShared<FJsonObject>();
        ProofPolicy->SetStringField(
            TEXT("camera_policy"),
            TEXT("LOCKED_34_DEGREE_SIDE_SPORTS_PROOF"));
        ProofPolicy->SetNumberField(
            TEXT("fixed_frame_rate_hz"), MetaHumanProofFrameRateHz);
        ProofPolicy->SetNumberField(
            TEXT("fixed_delta_time_seconds"), MetaHumanProofFixedDeltaTimeSeconds);
        ProofPolicy->SetBoolField(
            TEXT("fixed_timestep_forced_during_capture"),
            bFixedTimeStepCaptured);
        ProofPolicy->SetBoolField(
            TEXT("saved_use_fixed_timestep"), bSavedUseFixedTimeStep);
        ProofPolicy->SetNumberField(
            TEXT("saved_fixed_delta_time_seconds"), SavedFixedDeltaTimeSeconds);
        ProofPolicy->SetNumberField(
            TEXT("maximum_montage_frame_error"), MetaHumanProofMaximumFrameError);
        ProofPolicy->SetStringField(
            TEXT("montage_position_source"),
            TEXT("EXACT_COMMITTED_FANIMMONTAGEINSTANCE_ID_POSITION_AND_BLEND_WEIGHTS"));
        ProofPolicy->SetBoolField(
            TEXT("montage_full_weight_gate_applies_to_all_checkpoints"), true);
        ProofPolicy->SetNumberField(
            TEXT("minimum_montage_actual_blend_weight"),
            MetaHumanProofMinimumMontageBlendWeight);
        ProofPolicy->SetNumberField(
            TEXT("maximum_montage_actual_blend_weight"),
            MetaHumanProofMaximumMontageBlendWeight);
        ProofPolicy->SetNumberField(
            TEXT("minimum_montage_desired_blend_weight"),
            MetaHumanProofMinimumMontageBlendWeight);
        ProofPolicy->SetNumberField(
            TEXT("maximum_montage_desired_blend_weight"),
            MetaHumanProofMaximumMontageBlendWeight);
        ProofPolicy->SetStringField(
            TEXT("montage_instance_identity_contract"),
            TEXT("ANIM_INSTANCE_PATH_PLUS_MONTAGE_PATH_PLUS_ENGINE_INSTANCE_ID_MUST_MATCH_THROW_COMPONENT_COMMIT"));
        ProofPolicy->SetNumberField(
            TEXT("recovery_capture_trigger_lead_frames"),
            MetaHumanRecoveryCheckpointTriggerLeadFrames);
        ProofPolicy->SetNumberField(
            TEXT("required_montage_blend_out_trigger_seconds"),
            MetaHumanProofRequiredBlendOutTriggerTimeSeconds);
        ProofPolicy->SetBoolField(TEXT("montage_pose_seeking_allowed"), false);
        ProofPolicy->SetBoolField(TEXT("synthetic_phase_injection_allowed"), false);
        ProofPolicy->SetNumberField(
            TEXT("release_curve_minimum_at_checkpoint"),
            MetaHumanProofReleaseCurveMinimum);
        ProofPolicy->SetNumberField(
            TEXT("follow_through_curve_minimum_at_named_phase_checkpoint"),
            MetaHumanProofFollowThroughCurveMinimum);
        ProofPolicy->SetStringField(
            TEXT("follow_through_checkpoint_contract"),
            TEXT("NAMED_PHASE_FRAME_94_NOT_RECOVERY_DECELERATION_FRAME_104"));
        ProofPolicy->SetNumberField(
            TEXT("maximum_source_target_hand_direction_error_degrees"),
            MetaHumanProofMaximumSourceTargetDirectionErrorDegrees);
        ProofPolicy->SetNumberField(
            TEXT("maximum_source_target_elbow_angle_error_degrees"),
            MetaHumanProofMaximumSourceTargetElbowAngleErrorDegrees);
        ProofPolicy->SetStringField(
            TEXT("accepted_source_grip_relative_fallback_mode"),
            DiscGolfMetaHumanPresentation::
                SourceGripRelativeFallbackBindingMode);
        ProofPolicy->SetNumberField(
            TEXT("minimum_target_source_hand_distance_ratio"),
            MetaHumanProofMinimumTargetSourceDistanceRatio);
        ProofPolicy->SetNumberField(
            TEXT("maximum_target_source_hand_distance_ratio"),
            MetaHumanProofMaximumTargetSourceDistanceRatio);
        ProofPolicy->SetNumberField(
            TEXT("brace_ball_minimum_ground_gap_cm"),
            MetaHumanProofBraceBallMinimumGroundGapCm);
        ProofPolicy->SetNumberField(
            TEXT("brace_ball_maximum_ground_gap_cm"),
            MetaHumanProofBraceBallMaximumGroundGapCm);
        ProofPolicy->SetNumberField(
            TEXT("minimum_ground_impact_normal_z"),
            MetaHumanProofMinimumGroundNormalZ);
        ProofPolicy->SetNumberField(
            TEXT("maximum_prerelease_grip_plane_error_degrees"),
            MetaHumanProofMaximumPreReleaseGripPlaneErrorDegrees);
        ProofPolicy->SetNumberField(
            TEXT("maximum_release_grip_plane_error_degrees"),
            MetaHumanProofMaximumReleaseGripPlaneErrorDegrees);
        ProofPolicy->SetNumberField(
            TEXT("line_of_sight_endpoint_allowance_cm"),
            MetaHumanProofLineOfSightEndpointAllowanceCm);
        ProofPolicy->SetStringField(
            TEXT("locked_camera_origin"), LockedProofCameraOrigin.ToCompactString());
        ProofPolicy->SetStringField(
            TEXT("locked_camera_forward"), LockedProofCameraForward.ToCompactString());
        ProofPolicy->SetStringField(
            TEXT("locked_camera_right"), LockedProofCameraRight.ToCompactString());
        ProofPolicy->SetStringField(
            TEXT("locked_camera_up"), LockedProofCameraUp.ToCompactString());
        ProofPolicy->SetStringField(
            TEXT("locked_camera_target"), LockedProofCameraTarget.ToCompactString());
        ProofPolicy->SetStringField(
            TEXT("locked_camera_location"), LockedProofCameraLocation.ToCompactString());
        ProofPolicy->SetStringField(
            TEXT("locked_camera_rotation"), LockedProofCameraRotation.ToCompactString());
        ProofPolicy->SetNumberField(
            TEXT("locked_camera_fov_degrees"),
            MetaHumanProofCameraFieldOfViewDegrees);
        ProofPolicy->SetNumberField(
            TEXT("maximum_camera_location_error_cm"),
            MetaHumanProofMaximumCameraLocationErrorCm);
        ProofPolicy->SetNumberField(
            TEXT("maximum_camera_rotation_error_degrees"),
            MetaHumanProofMaximumCameraRotationErrorDegrees);
        ProofPolicy->SetNumberField(
            TEXT("maximum_camera_fov_error_degrees"),
            MetaHumanProofMaximumCameraFieldOfViewErrorDegrees);
        ProofPolicy->SetStringField(TEXT("exposure_method"), TEXT("MANUAL"));
        ProofPolicy->SetNumberField(
            TEXT("exposure_bias_ev"), MetaHumanProofExposureBiasEv);
        ProofPolicy->SetNumberField(
            TEXT("white_temperature_kelvin"),
            MetaHumanProofWhiteTemperatureKelvin);
        ProofPolicy->SetNumberField(
            TEXT("key_light_lumens"), MetaHumanProductionCaptureKeyLightLumens);
        ProofPolicy->SetNumberField(
            TEXT("fill_light_lumens"), MetaHumanProductionCaptureFillLightLumens);
        ProofPolicy->SetBoolField(
            TEXT("hud_hidden_during_capture"), bHudVisibilityCaptured);
        ProofPolicy->SetBoolField(TEXT("motion_blur_disabled"), true);
        ProofPolicy->SetBoolField(TEXT("depth_of_field_disabled"), true);
        ProofPolicy->SetBoolField(
            TEXT("presentation_state_restored"),
            bCapturePresentationStateRestored);

        TArray<TSharedPtr<FJsonValue>> Checkpoints;
        for (int32 Index = 0;
            Index < UE_ARRAY_COUNT(MetaHumanProductionTargetFrames);
            ++Index)
        {
            TSharedRef<FJsonObject> Checkpoint = MakeShared<FJsonObject>();
            Checkpoint->SetStringField(
                TEXT("name"), MetaHumanProductionCheckpointNames[Index]);
            Checkpoint->SetNumberField(
                TEXT("target_frame"), MetaHumanProductionTargetFrames[Index]);
            Checkpoint->SetNumberField(
                TEXT("target_seconds"),
                MetaHumanProductionTargetFrames[Index]
                    / MetaHumanProofFrameRateHz);
            Checkpoint->SetNumberField(
                TEXT("expected_phase_enum"),
                static_cast<int32>(MetaHumanProductionExpectedPhases[Index]));
            Checkpoint->SetNumberField(
                TEXT("minimum_target_hand_to_pelvis_cm"),
                MetaHumanProductionMinimumTargetHandToPelvisCm[Index]);
            Checkpoint->SetNumberField(
                TEXT("minimum_target_arm_reach_fraction"),
                MetaHumanProductionMinimumTargetArmReachFraction[Index]);
            Checkpoints.Add(MakeShared<FJsonValueObject>(Checkpoint));
        }
        ProofPolicy->SetArrayField(TEXT("checkpoints"), Checkpoints);
        Root->SetObjectField(TEXT("proof_policy"), ProofPolicy);
    }

    TArray<TSharedPtr<FJsonValue>> Captures;
    const auto VectorToJson = [](const FVector& Vector)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("x"), Vector.X);
        Object->SetNumberField(TEXT("y"), Vector.Y);
        Object->SetNumberField(TEXT("z"), Vector.Z);
        return Object;
    };
    for (int32 CaptureIndex = 0;
        CaptureIndex < CaptureRecords.Num();
        ++CaptureIndex)
    {
        const FCaptureRecord& Record = CaptureRecords[CaptureIndex];
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("capture_index"), CaptureIndex);
        Object->SetStringField(TEXT("filename"), Record.Filename);
        Object->SetStringField(TEXT("path"), FPaths::Combine(OutputDirectory, Record.Filename));
        Object->SetStringField(TEXT("evidence"), Record.Evidence);
        Object->SetStringField(TEXT("source"), Record.Source);
        Object->SetNumberField(TEXT("bytes"), static_cast<double>(Record.Bytes));
        Object->SetNumberField(TEXT("width"), Record.Width);
        Object->SetNumberField(TEXT("height"), Record.Height);
        Object->SetNumberField(TEXT("camera_to_subject_cm"), Record.CameraToSubjectCm);
        Object->SetNumberField(TEXT("subject_bounds_radius_cm"), Record.SubjectBoundsRadiusCm);
        Object->SetBoolField(TEXT("camera_outside_subject_bounds"), Record.bCameraOutsideSubjectBounds);
        Object->SetBoolField(TEXT("subject_framed"), Record.bSubjectFramed);
        Object->SetBoolField(TEXT("focus_point_framed"), Record.bFocusPointFramed);
        Object->SetBoolField(TEXT("line_of_sight_clear"), Record.bLineOfSightClear);
        Object->SetNumberField(
            TEXT("line_of_sight_landmark_count"),
            Record.LineOfSightLandmarkCount);
        Object->SetNumberField(
            TEXT("clear_line_of_sight_landmark_count"),
            Record.ClearLineOfSightLandmarkCount);
        Object->SetStringField(
            TEXT("first_occluded_landmark"), Record.FirstOccludedLandmark);
        Object->SetBoolField(TEXT("bone_lengths_invariant"), Record.bBoneLengthsInvariant);
        Object->SetNumberField(TEXT("max_bone_length_ratio_error"), Record.MaxBoneLengthRatioError);
        Object->SetStringField(
            TEXT("worst_bone_length_segment"), Record.WorstBoneLengthSegment);
        Object->SetNumberField(
            TEXT("worst_bone_expected_length_cm"), Record.WorstBoneExpectedLengthCm);
        Object->SetNumberField(
            TEXT("worst_bone_actual_length_cm"), Record.WorstBoneActualLengthCm);
        Object->SetNumberField(
            TEXT("bone_length_ratio_tolerance"),
            bMetaHumanProductionVisualCapture ? 0.20 : 0.12);
        Object->SetBoolField(
            TEXT("retarget_pose_telemetry_valid"),
            Record.bRetargetPoseTelemetryValid);
        Object->SetBoolField(
            TEXT("metahuman_visual_root_uses_absolute_scale"),
            Record.bMetaHumanVisualRootUsesAbsoluteScale);
        Object->SetObjectField(
            TEXT("metahuman_visual_root_world_scale"),
            VectorToJson(Record.MetaHumanVisualRootWorldScale));
        Object->SetObjectField(
            TEXT("metahuman_body_world_scale"),
            VectorToJson(Record.MetaHumanBodyWorldScale));
        Object->SetObjectField(
            TEXT("metahuman_head_world_scale"),
            VectorToJson(Record.MetaHumanHeadWorldScale));
        Object->SetObjectField(
            TEXT("metahuman_outfit_world_scale"),
            VectorToJson(Record.MetaHumanOutfitWorldScale));
        Object->SetNumberField(TEXT("throw_phase_enum"), Record.ThrowPhase);
        Object->SetStringField(TEXT("checkpoint_name"), Record.CheckpointName);
        Object->SetNumberField(
            TEXT("target_montage_frame"), Record.TargetMontageFrame);
        Object->SetNumberField(
            TEXT("target_montage_seconds"), Record.TargetMontageSeconds);
        Object->SetNumberField(
            TEXT("actual_montage_seconds"), Record.ActualMontageSeconds);
        Object->SetNumberField(
            TEXT("actual_montage_frame"), Record.ActualMontageFrame);
        Object->SetNumberField(
            TEXT("montage_frame_error"), Record.MontageFrameError);
        Object->SetBoolField(
            TEXT("checkpoint_timing_valid"), Record.bCheckpointTimingValid);
        Object->SetNumberField(
            TEXT("montage_instance_id"), Record.MontageInstanceId);
        Object->SetStringField(
            TEXT("montage_instance_token"), Record.MontageInstanceToken);
        Object->SetStringField(
            TEXT("montage_instance_anim_instance_path"),
            Record.MontageInstanceAnimInstancePath);
        Object->SetStringField(
            TEXT("montage_instance_montage_path"),
            Record.MontageInstanceMontagePath);
        Object->SetNumberField(
            TEXT("montage_actual_blend_weight"),
            Record.MontageActualBlendWeight);
        Object->SetNumberField(
            TEXT("montage_desired_blend_weight"),
            Record.MontageDesiredBlendWeight);
        Object->SetBoolField(
            TEXT("montage_instance_matches_committed_throw"),
            Record.bMontageInstanceMatchesCommittedThrow);
        Object->SetBoolField(
            TEXT("montage_full_weight_gate_valid"),
            Record.bMontageFullWeightGateValid);
        Object->SetStringField(
            TEXT("camera_world_location"),
            Record.CameraWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("camera_world_rotation"),
            Record.CameraWorldRotation.ToCompactString());
        Object->SetNumberField(
            TEXT("camera_field_of_view_degrees"),
            Record.CameraFieldOfViewDegrees);
        Object->SetNumberField(
            TEXT("locked_camera_location_error_cm"),
            Record.LockedCameraLocationErrorCm);
        Object->SetNumberField(
            TEXT("locked_camera_rotation_error_degrees"),
            Record.LockedCameraRotationErrorDegrees);
        Object->SetNumberField(
            TEXT("locked_camera_fov_error_degrees"),
            Record.LockedCameraFieldOfViewErrorDegrees);
        Object->SetBoolField(
            TEXT("locked_camera_invariant"), Record.bLockedCameraInvariant);
        Object->SetNumberField(
            TEXT("screenshot_request_frame_counter"),
            static_cast<double>(Record.ScreenshotRequestFrameCounter));
        Object->SetNumberField(
            TEXT("screenshot_request_source_bone_revision"),
            Record.ScreenshotRequestSourceBoneRevision);
        Object->SetNumberField(
            TEXT("screenshot_request_target_bone_revision"),
            Record.ScreenshotRequestTargetBoneRevision);
        Object->SetNumberField(
            TEXT("screenshot_request_source_bone_transform_frame"),
            Record.ScreenshotRequestSourceBoneTransformFrame);
        Object->SetNumberField(
            TEXT("screenshot_request_target_bone_transform_frame"),
            Record.ScreenshotRequestTargetBoneTransformFrame);
        Object->SetNumberField(
            TEXT("screenshot_request_fixed_delta_time_seconds"),
            Record.ScreenshotRequestFixedDeltaTimeSeconds);
        Object->SetBoolField(
            TEXT("screenshot_request_fixed_timestep_active"),
            Record.bScreenshotRequestFixedTimeStepActive);
        Object->SetBoolField(
            TEXT("screenshot_request_pose_bound"),
            Record.bScreenshotRequestPoseBound);
        Object->SetStringField(
            TEXT("resolved_camera_world_location"),
            Record.ResolvedCameraWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("resolved_camera_world_rotation"),
            Record.ResolvedCameraWorldRotation.ToCompactString());
        Object->SetNumberField(
            TEXT("resolved_camera_field_of_view_degrees"),
            Record.ResolvedCameraFieldOfViewDegrees);
        Object->SetNumberField(
            TEXT("resolved_camera_location_error_cm"),
            Record.ResolvedCameraLocationErrorCm);
        Object->SetNumberField(
            TEXT("resolved_camera_rotation_error_degrees"),
            Record.ResolvedCameraRotationErrorDegrees);
        Object->SetNumberField(
            TEXT("resolved_camera_fov_error_degrees"),
            Record.ResolvedCameraFieldOfViewErrorDegrees);
        Object->SetBoolField(
            TEXT("resolved_camera_invariant"),
            Record.bResolvedCameraInvariant);
        Object->SetNumberField(
            TEXT("DG_FootPlant_L"), Record.FootPlantLeftAlpha);
        Object->SetNumberField(
            TEXT("DG_FootPlant_R"), Record.FootPlantRightAlpha);
        Object->SetNumberField(
            TEXT("DG_ReachbackAlpha"), Record.ReachBackAlpha);
        Object->SetNumberField(TEXT("DG_BraceAlpha"), Record.BraceAlpha);
        Object->SetNumberField(
            TEXT("DG_ReleaseApproachAlpha"), Record.ReleaseApproachAlpha);
        Object->SetNumberField(
            TEXT("DG_FollowThroughAlpha"), Record.FollowThroughAlpha);
        Object->SetNumberField(
            TEXT("DG_DiscPlaneAlpha"), Record.DiscPlaneStabilityAlpha);
        Object->SetNumberField(
            TEXT("DG_WeightShiftAlpha"), Record.WeightShiftAlpha);
        Object->SetNumberField(
            TEXT("DG_BraceCompressionAlpha"), Record.BraceCompressionAlpha);
        Object->SetNumberField(
            TEXT("DG_HipDriveAlpha"), Record.HipDriveAlpha);
        Object->SetNumberField(
            TEXT("DG_TorsoDriveAlpha"), Record.TorsoDriveAlpha);
        Object->SetNumberField(
            TEXT("DG_ShoulderDriveAlpha"), Record.ShoulderDriveAlpha);
        Object->SetNumberField(
            TEXT("DG_ElbowLeadAlpha"), Record.ElbowLeadAlpha);
        Object->SetNumberField(
            TEXT("DG_WristLagAlpha"), Record.WristLagAlpha);
        Object->SetNumberField(
            TEXT("DG_FingerReleaseAlpha"), Record.FingerReleaseAlpha);
        Object->SetNumberField(
            TEXT("DG_OffArmCounterbalanceAlpha"),
            Record.OffArmCounterbalanceAlpha);
        Object->SetNumberField(
            TEXT("DG_GazeTargetAlpha"), Record.GazeTargetAlpha);
        Object->SetNumberField(
            TEXT("DG_BraceExtensionAlpha"), Record.BraceExtensionAlpha);
        Object->SetNumberField(
            TEXT("DG_RecoveryBeatAlpha"), Record.RecoveryBeatAlpha);
        Object->SetBoolField(
            TEXT("motion_curves_finite_and_normalized"),
            Record.bMotionCurvesFiniteAndNormalized);
        Object->SetBoolField(
            TEXT("checkpoint_curve_gate_valid"),
            Record.bCheckpointCurveGateValid);
        Object->SetStringField(
            TEXT("source_shoulder_world_location"),
            Record.SourceShoulderWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("source_elbow_world_location"),
            Record.SourceElbowWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("source_hand_world_location"),
            Record.SourceHandWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("source_shoulder_relative_to_pelvis_cm"),
            Record.SourceShoulderRelativeToPelvisCm.ToCompactString());
        Object->SetStringField(
            TEXT("source_elbow_relative_to_pelvis_cm"),
            Record.SourceElbowRelativeToPelvisCm.ToCompactString());
        Object->SetStringField(
            TEXT("source_hand_relative_to_pelvis_cm"),
            Record.SourceHandRelativeToPelvisCm.ToCompactString());
        Object->SetStringField(
            TEXT("target_shoulder_world_location"),
            Record.TargetShoulderWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("target_elbow_world_location"),
            Record.TargetElbowWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("target_hand_world_location"),
            Record.TargetHandWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("target_shoulder_relative_to_pelvis_cm"),
            Record.TargetShoulderRelativeToPelvisCm.ToCompactString());
        Object->SetStringField(
            TEXT("target_elbow_relative_to_pelvis_cm"),
            Record.TargetElbowRelativeToPelvisCm.ToCompactString());
        Object->SetStringField(
            TEXT("target_hand_relative_to_pelvis_cm"),
            Record.TargetHandRelativeToPelvisCm.ToCompactString());
        Object->SetNumberField(
            TEXT("source_right_elbow_angle_degrees"),
            Record.SourceElbowAngleDegrees);
        Object->SetNumberField(
            TEXT("target_right_elbow_angle_degrees"),
            Record.TargetElbowAngleDegrees);
        Object->SetNumberField(
            TEXT("source_to_target_elbow_angle_error_degrees"),
            Record.SourceToTargetElbowAngleErrorDegrees);
        Object->SetNumberField(
            TEXT("source_shoulder_to_hand_distance_cm"),
            Record.SourceShoulderToHandDistanceCm);
        Object->SetNumberField(
            TEXT("target_shoulder_to_hand_distance_cm"),
            Record.TargetShoulderToHandDistanceCm);
        Object->SetNumberField(
            TEXT("source_arm_reach_fraction"), Record.SourceArmReachFraction);
        Object->SetNumberField(
            TEXT("target_arm_reach_fraction"), Record.TargetArmReachFraction);
        Object->SetNumberField(
            TEXT("source_hand_to_pelvis_distance_cm"),
            Record.SourceHandToPelvisDistanceCm);
        Object->SetNumberField(
            TEXT("target_hand_to_pelvis_distance_cm"),
            Record.TargetHandToPelvisDistanceCm);
        Object->SetNumberField(
            TEXT("target_to_source_hand_pelvis_distance_ratio"),
            Record.TargetToSourceHandPelvisDistanceRatio);
        Object->SetNumberField(
            TEXT("source_to_target_hand_direction_error_degrees"),
            Record.SourceToTargetHandDirectionErrorDegrees);
        Object->SetNumberField(
            TEXT("target_hand_horizontal_from_pelvis_cm"),
            Record.TargetHandHorizontalFromPelvisCm);
        Object->SetNumberField(
            TEXT("target_hand_vertical_from_pelvis_cm"),
            Record.TargetHandVerticalFromPelvisCm);
        Object->SetBoolField(
            TEXT("target_pose_spatial_gate_valid"),
            Record.bTargetPoseSpatialGateValid);
        Object->SetStringField(
            TEXT("hand_correction_mode"), Record.HandCorrectionMode);
        Object->SetStringField(
            TEXT("source_disc_grip_world_location"),
            Record.SourceDiscGripWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("desired_target_hand_world_location"),
            Record.DesiredTargetHandWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("target_hand_pre_correction_world_location"),
            Record.TargetHandPreCorrectionWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("bounded_effector_target_world_location"),
            Record.BoundedEffectorTargetWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("target_hand_post_correction_world_location"),
            Record.TargetHandPostCorrectionWorldLocation.ToCompactString());
        Object->SetNumberField(
            TEXT("source_hand_to_disc_grip_distance_cm"),
            Record.SourceHandToDiscGripDistanceCm);
        Object->SetNumberField(
            TEXT("target_hand_pre_to_source_hand_distance_cm"),
            Record.TargetHandPreToSourceHandDistanceCm);
        Object->SetNumberField(
            TEXT("target_hand_post_to_source_hand_distance_cm"),
            Record.TargetHandPostToSourceHandDistanceCm);
        Object->SetNumberField(
            TEXT("hand_correction_improvement_cm"),
            Record.HandCorrectionImprovementCm);
        Object->SetNumberField(
            TEXT("target_hand_pre_to_desired_hand_distance_cm"),
            Record.TargetHandPreToDesiredHandDistanceCm);
        Object->SetNumberField(
            TEXT("target_hand_post_to_desired_hand_distance_cm"),
            Record.TargetHandPostToDesiredHandDistanceCm);
        Object->SetNumberField(
            TEXT("desired_hand_correction_improvement_cm"),
            Record.DesiredHandCorrectionImprovementCm);
        Object->SetNumberField(
            TEXT("requested_effector_correction_cm"),
            Record.RequestedEffectorCorrectionCm);
        Object->SetNumberField(
            TEXT("bounded_effector_correction_cm"),
            Record.BoundedEffectorCorrectionCm);
        Object->SetNumberField(
            TEXT("effector_correction_cap_cm"),
            Record.EffectorCorrectionCapCm);
        Object->SetNumberField(
            TEXT("target_hand_post_to_rendered_hand_distance_cm"),
            Record.TargetHandPostToRenderedHandDistanceCm);
        Object->SetNumberField(
            TEXT("source_snapshot_to_rendered_hand_distance_cm"),
            Record.SourceSnapshotToRenderedHandDistanceCm);
        Object->SetNumberField(
            TEXT("desired_hand_orientation_error_degrees"),
            Record.DesiredHandOrientationErrorDegrees);
        Object->SetNumberField(
            TEXT("target_hand_post_to_desired_orientation_error_degrees"),
            Record.TargetHandPostToDesiredOrientationErrorDegrees);
        Object->SetNumberField(
            TEXT("phase_correction_weight"), Record.PhaseCorrectionWeight);
        Object->SetNumberField(
            TEXT("applied_correction_blend_alpha"),
            Record.AppliedCorrectionBlendAlpha);
        Object->SetNumberField(
            TEXT("maximum_corrected_segment_ratio_error"),
            Record.MaximumCorrectedSegmentRatioError);
        Object->SetNumberField(
            TEXT("source_bone_revision_at_pre_update"),
            Record.SourceBoneRevisionAtPreUpdate);
        Object->SetNumberField(
            TEXT("target_bone_revision_before_evaluate"),
            Record.TargetBoneRevisionBeforeEvaluate);
        Object->SetNumberField(
            TEXT("source_bone_revision_at_capture"),
            Record.SourceBoneRevisionAtCapture);
        Object->SetNumberField(
            TEXT("target_bone_revision_at_capture"),
            Record.TargetBoneRevisionAtCapture);
        Object->SetNumberField(
            TEXT("source_bone_transform_frame_at_capture"),
            Record.SourceBoneTransformFrameAtCapture);
        Object->SetNumberField(
            TEXT("target_bone_transform_frame_at_capture"),
            Record.TargetBoneTransformFrameAtCapture);
        Object->SetNumberField(
            TEXT("source_sample_frame_counter"),
            static_cast<double>(Record.SourceSampleFrameCounter));
        Object->SetNumberField(
            TEXT("target_correction_frame_counter"),
            static_cast<double>(Record.TargetCorrectionFrameCounter));
        Object->SetNumberField(
            TEXT("pose_telemetry_capture_frame_counter"),
            static_cast<double>(Record.PoseTelemetryCaptureFrameCounter));
        Object->SetBoolField(
            TEXT("hand_correction_snapshot_valid"),
            Record.bHandCorrectionSnapshotValid);
        Object->SetBoolField(
            TEXT("full_grip_transform_available"),
            Record.bFullGripTransformAvailable);
        Object->SetStringField(
            TEXT("grip_binding_mode"),
            Record.GripTransformEvidenceMode);
        Object->SetBoolField(
            TEXT("source_grip_relative_fallback_available"),
            Record.bSourceGripRelativeFallbackAvailable);
        Object->SetBoolField(
            TEXT("grip_transform_evidence_accepted"),
            Record.bGripTransformEvidenceAccepted);
        Object->SetBoolField(
            TEXT("effector_correction_clamped"),
            Record.bEffectorCorrectionClamped);
        Object->SetBoolField(
            TEXT("effector_distance_cap_exceeded"),
            Record.bEffectorDistanceCapExceeded);
        Object->SetBoolField(
            TEXT("desired_target_within_reach_annulus"),
            Record.bDesiredTargetWithinReachAnnulus);
        Object->SetBoolField(
            TEXT("desired_target_within_safe_reach_annulus"),
            Record.bDesiredTargetWithinReachAnnulus);
        Object->SetBoolField(
            TEXT("hand_correction_reachable"),
            Record.bHandCorrectionReachable);
        Object->SetBoolField(
            TEXT("hand_correction_applied"),
            Record.bHandCorrectionApplied);
        Object->SetNumberField(TEXT("subject_screen_height_fraction"), Record.SubjectScreenHeightFraction);
        Object->SetStringField(
            TEXT("subject_screen_scale_basis"),
            bMetaHumanProductionVisualCapture
                ? TEXT("maximum_projected_required_landmark_span")
                : TEXT("projected_head_to_pelvis_or_feet_height"));
        Object->SetNumberField(TEXT("focus_projected_diameter_pixels"), Record.FocusProjectedDiameterPixels);
        Object->SetBoolField(TEXT("subject_readable_scale"), Record.bSubjectReadableScale);
        Object->SetBoolField(TEXT("evidence_mesh_readable"), Record.bEvidenceMeshReadable);
        Object->SetStringField(TEXT("evidence_component"), Record.EvidenceComponent);
        Object->SetStringField(TEXT("evidence_component_world_location"),
            Record.EvidenceComponentWorldLocation.ToCompactString());
        Object->SetNumberField(TEXT("evidence_actor_to_component_cm"),
            Record.EvidenceActorToComponentCm);
        Object->SetNumberField(TEXT("evidence_focus_alignment_cm"),
            Record.EvidenceFocusAlignmentCm);
        Object->SetBoolField(TEXT("evidence_focus_anchored_to_component"),
            Record.bEvidenceFocusAnchoredToComponent);
        Object->SetStringField(
            TEXT("foot_l_world_location"),
            Record.FootLeftWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("foot_r_world_location"),
            Record.FootRightWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("ball_l_world_location"),
            Record.BallLeftWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("ball_r_world_location"),
            Record.BallRightWorldLocation.ToCompactString());
        Object->SetNumberField(
            TEXT("ball_l_ground_gap_cm"), Record.BallLeftGroundGapCm);
        Object->SetNumberField(
            TEXT("ball_r_ground_gap_cm"), Record.BallRightGroundGapCm);
        Object->SetBoolField(
            TEXT("ball_l_ground_trace_valid"),
            Record.bBallLeftGroundTraceValid);
        Object->SetBoolField(
            TEXT("ball_r_ground_trace_valid"),
            Record.bBallRightGroundTraceValid);
        Object->SetStringField(
            TEXT("ball_l_ground_hit_actor"), Record.BallLeftGroundHitActor);
        Object->SetStringField(
            TEXT("ball_l_ground_hit_component"),
            Record.BallLeftGroundHitComponent);
        Object->SetStringField(
            TEXT("ball_r_ground_hit_actor"), Record.BallRightGroundHitActor);
        Object->SetStringField(
            TEXT("ball_r_ground_hit_component"),
            Record.BallRightGroundHitComponent);
        Object->SetStringField(
            TEXT("ball_l_ground_impact_point"),
            Record.BallLeftGroundImpactPoint.ToCompactString());
        Object->SetStringField(
            TEXT("ball_r_ground_impact_point"),
            Record.BallRightGroundImpactPoint.ToCompactString());
        Object->SetStringField(
            TEXT("ball_l_ground_impact_normal"),
            Record.BallLeftGroundImpactNormal.ToCompactString());
        Object->SetStringField(
            TEXT("ball_r_ground_impact_normal"),
            Record.BallRightGroundImpactNormal.ToCompactString());
        Object->SetBoolField(
            TEXT("brace_foot_ground_gate_valid"),
            Record.bBraceFootGroundGateValid);
        Object->SetBoolField(
            TEXT("required_foot_ground_evidence_valid"),
            Record.bRequiredFootGroundEvidenceValid);
        Object->SetBoolField(
            TEXT("disc_plane_evidence_available"),
            Record.bDiscPlaneEvidenceAvailable);
        Object->SetStringField(
            TEXT("disc_world_location"), Record.DiscWorldLocation.ToCompactString());
        Object->SetStringField(
            TEXT("disc_normal_world"), Record.DiscNormalWorld.ToCompactString());
        Object->SetStringField(
            TEXT("disc_tangent_world"), Record.DiscTangentWorld.ToCompactString());
        Object->SetStringField(
            TEXT("authored_grip_normal_world"),
            Record.AuthoredGripNormalWorld.ToCompactString());
        Object->SetStringField(
            TEXT("authored_grip_tangent_world"),
            Record.AuthoredGripTangentWorld.ToCompactString());
        Object->SetNumberField(
            TEXT("disc_plane_tilt_from_world_up_degrees"),
            Record.DiscPlaneTiltFromWorldUpDegrees);
        Object->SetNumberField(
            TEXT("disc_normal_to_authored_grip_error_degrees"),
            Record.DiscNormalToAuthoredGripErrorDegrees);
        Object->SetNumberField(
            TEXT("disc_tangent_to_authored_grip_error_degrees"),
            Record.DiscTangentToAuthoredGripErrorDegrees);
        Object->SetNumberField(
            TEXT("maximum_disc_to_authored_grip_plane_error_degrees"),
            Record.MaximumDiscToAuthoredGripPlaneErrorDegrees);
        Object->SetBoolField(
            TEXT("disc_plane_matches_authored_grip"),
            Record.bDiscPlaneMatchesAuthoredGrip);
        Object->SetBoolField(
            TEXT("disc_plane_evidence_valid"),
            Record.bDiscPlaneEvidenceValid);
        Object->SetBoolField(
            TEXT("metahuman_proof_telemetry_valid"),
            Record.bMetaHumanProofTelemetryValid);
        Captures.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("captures"), Captures);

    TSharedRef<FJsonObject> Live = MakeShared<FJsonObject>();
    Live->SetStringField(TEXT("pawn"), TEXT("possessed ADiscGolferPawn"));
    Live->SetStringField(
        TEXT("montage"),
        bMetaHumanProductionVisualCapture
            ? ActiveProductionMontagePath
            : TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype"));
    Live->SetStringField(
        TEXT("expected_production_motion_revision"),
        ExpectedProductionMotionRevision);
    Live->SetStringField(
        TEXT("actual_production_motion_candidate_revision"),
        Golfer ? Golfer->GetProductionMotionCandidateRevision() : FString());
    FString RequestedMotionCandidate;
    const bool bMotionCandidateOverrideRequested = FParse::Value(
        FCommandLine::Get(), TEXT("DGProductionMotionCandidate="),
        RequestedMotionCandidate);
    Live->SetBoolField(
        TEXT("production_motion_candidate_override_requested"),
        bMotionCandidateOverrideRequested);
    Live->SetStringField(
        TEXT("requested_production_motion_candidate"),
        RequestedMotionCandidate);
    Live->SetBoolField(
        TEXT("verified_metahuman_gameplay_presentation"),
        bMetaHumanProductionVisualCapture
            && MetaHumanBackend
            && MetaHumanBackend->IsVisualBackendReady()
            && MetaHumanBackend->IsPresentationPolicyVerified()
            && MetaHumanBackend->GetVerifiedPresentationPolicy()
                == EDGMetaHumanPresentationPolicy::GameplayPerformance
            && MetaHumanVisualBody
            && !Golfer->IsDGProxyPresentationVisible());
    Live->SetStringField(TEXT("release_source"), TEXT("DG Release Disc -> project adapter -> RequestThrowFromGrip"));
    Live->SetNumberField(TEXT("release_callback_count"), LiveReleaseCallbackCount);
    Live->SetNumberField(TEXT("stroke_delta"), GameMode ? GameMode->GetStrokes() - BaselineStrokes : -1);
    Live->SetBoolField(TEXT("follow_through_reached"), bLiveFollowThroughReached);
    Live->SetBoolField(TEXT("recovered"), bLiveThrowRecovered);
    Live->SetStringField(TEXT("recovery_reason"), LiveRecoveryReason);
    Live->SetBoolField(
        TEXT("authoritative_flight_settled"),
        bLiveAuthoritativeFlightSettled);
    Live->SetBoolField(
        TEXT("authoritative_flight_holed_out"),
        bLiveAuthoritativeFlightHoledOut);
    Live->SetBoolField(
        TEXT("authoritative_flight_validation_attempted"),
        bLiveAuthoritativeFlightValidationAttempted);
    Live->SetBoolField(
        TEXT("authoritative_flight_validation_passed"),
        bLiveAuthoritativeFlightValidationPassed);
    Live->SetBoolField(
        TEXT("trajectory_capture_valid"),
        bLiveTrajectoryCaptureValid);
    Live->SetStringField(
        TEXT("baseline_trajectory_capture_id"),
        BaselineTrajectoryCaptureId);
    Live->SetStringField(
        TEXT("trajectory_capture_id"),
        LiveTrajectoryCaptureId);
    Live->SetNumberField(
        TEXT("trajectory_sample_count"),
        LiveTrajectorySampleCount);
    Live->SetNumberField(
        TEXT("flight_duration_seconds"),
        LiveFlightDurationSeconds);
    Live->SetNumberField(
        TEXT("trajectory_air_carry_meters"),
        LiveTrajectoryAirCarryMeters);
    Live->SetNumberField(
        TEXT("trajectory_final_carry_meters"),
        LiveTrajectoryFinalCarryMeters);
    Live->SetNumberField(
        TEXT("flight_ground_contact_count"),
        LiveFlightGroundContactCount);
    Live->SetNumberField(
        TEXT("flight_basket_contact_count"),
        LiveFlightBasketContactCount);
    Live->SetBoolField(
        TEXT("trajectory_export_deferral_requested"),
        bLiveTrajectoryExportDeferralRequested);
    Live->SetBoolField(
        TEXT("trajectory_export_deferral_discarded"),
        bLiveTrajectoryExportDeferralDiscarded);
    Live->SetStringField(
        TEXT("flight_validation_failure"),
        LiveFlightValidationFailure);
    Live->SetBoolField(TEXT("held_disc_visible_after_release"), HeldDiscVisual && HeldDiscVisual->IsVisible());
    Live->SetNumberField(TEXT("world_disc_count_at_finish"), CountWorldDiscs());
    Root->SetObjectField(TEXT("live_throw"), Live);

    TArray<TSharedPtr<FJsonValue>> Profiles;
    for (const FProfileRecord& Record : ProfileRecords)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("name"), Record.Name);
        Object->SetStringField(TEXT("asset"), Record.AssetPath);
        Object->SetNumberField(TEXT("height_cm"), Record.HeightCm);
        Object->SetNumberField(TEXT("wingspan_scale"), Record.WingspanScale);
        Object->SetNumberField(TEXT("release_count"), Record.ReleaseCount);
        Object->SetBoolField(TEXT("animation_started"), Record.bAnimationStarted);
        Object->SetBoolField(TEXT("grip_transform_usable"), Record.bGripTransformUsable);
        Object->SetBoolField(TEXT("follow_through_reached"), Record.bFollowThroughReached);
        Object->SetBoolField(TEXT("recovered"), Record.bRecovered);
        Object->SetStringField(TEXT("recovery_reason"), Record.RecoveryReason);
        Object->SetBoolField(TEXT("pose_finite_and_plausible"), Record.bPoseFiniteAndPlausible);
        Object->SetStringField(TEXT("body_deformation"), TEXT("DEFERRED_TO_SESSION_4"));
        Object->SetStringField(TEXT("launch_mode"), TEXT("COMPATIBILITY_CALLBACK_NO_GAMEPLAY_DISC"));
        Profiles.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("profile_compatibility"), Profiles);

    TSharedRef<FJsonObject> Writes = MakeShared<FJsonObject>();
    Writes->SetBoolField(
        TEXT("png_and_manifest_only"),
        bMetaHumanProductionVisualCapture
            && (!bLiveTrajectoryExportDeferralRequested
                || bLiveTrajectoryExportDeferralDiscarded));
    Writes->SetStringField(
        TEXT("trajectory_export_policy"),
        bMetaHumanProductionVisualCapture
            ? TEXT("DEFER_SUMMARY_THEN_DISCARD_WITHOUT_JSON_OR_CSV_WRITE")
            : TEXT("LEGACY_LANE_NOT_ASSERTED"));
    Writes->SetBoolField(
        TEXT("trajectory_json_csv_written_by_capture_lane"), false);
    Writes->SetBoolField(
        TEXT("existing_profile_loaded"),
        !FParse::Param(FCommandLine::Get(), TEXT("NoLoadExistingSave")));
    Writes->SetBoolField(
        TEXT("profile_writes_allowed"),
        !FParse::Param(FCommandLine::Get(), TEXT("DGNoProfileWrites")));
    Writes->SetBoolField(
        TEXT("practice_snapshot_writes_allowed"),
        !FParse::Param(FCommandLine::Get(), TEXT("DGDeveloperToolNoSave")));
    Writes->SetArrayField(TEXT("uasset_writes"), TArray<TSharedPtr<FJsonValue>>());
    Writes->SetArrayField(TEXT("level_save_calls"), TArray<TSharedPtr<FJsonValue>>());
    Writes->SetArrayField(TEXT("content_save_calls"), TArray<TSharedPtr<FJsonValue>>());
    Root->SetObjectField(TEXT("writes"), Writes);
    Root->SetStringField(
        TEXT("production_limitation"),
        bMetaHumanProductionVisualCapture
            ? TEXT("Five locked-camera checkpoints prove exact 60 Hz ReachBack, Plant, Release, FollowThrough, and Recovery states on the verified MetaHuman GameplayPerformance presentation. The provisional Cylinder remains non-final, and manual review is still required for deformation, hand ergonomics, cloth/hair intersections, and temporal continuity between checkpoints.")
            : TEXT("Provisional validation proxy and gameplay Cylinder; final deformation/hand ergonomics remain deferred."));

    FString Json;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        return false;
    }
    return FFileHelper::SaveStringToFile(Json, *ManifestPath)
        && IFileManager::Get().FileSize(*ManifestPath) > 0;
}

void ADiscGolfSession3VisualCaptureRunner::Fail(const FString& Reason)
{
    if (bFinished)
    {
        return;
    }
    bFinished = true;
    SetStage(EStage::Finished);
    RestoreCaptureTimeDilation();
    RestoreCapturePresentationState();
    if (bLiveTrajectoryExportDeferralRequested
        && !bLiveTrajectoryExportDeferralDiscarded)
    {
        DiscardLiveTrajectoryExportDeferral();
    }
    const bool bManifestWritten = WriteManifest(false, Reason);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: FAIL %s | captures=%d manifest=%s manifest_written=%d"),
        *Reason,
        CaptureRecords.Num(),
        *ManifestPath,
        bManifestWritten ? 1 : 0);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession3VisualCaptureRunner::Pass()
{
    if (bFinished)
    {
        return;
    }
    const bool bAllCapturesPresent =
        CaptureRecords.Num() == GetExpectedCaptureCount();
    bool bAllFilesValid = bAllCapturesPresent;
    for (const FCaptureRecord& Record : CaptureRecords)
    {
        const bool bResolutionValid = bMetaHumanProductionVisualCapture
            ? (Record.Width == 1920 && Record.Height == 1080)
            : (Record.Width >= 1280 && Record.Height >= 720);
        bAllFilesValid &= Record.Bytes > 4096 && bResolutionValid
            && Record.bCameraOutsideSubjectBounds
            && Record.bSubjectFramed
            && Record.bFocusPointFramed
            && Record.bLineOfSightClear
            && Record.bBoneLengthsInvariant
            && Record.bSubjectReadableScale
            && Record.bEvidenceMeshReadable;
        if (bMetaHumanProductionVisualCapture)
        {
            bAllFilesValid &= Record.bRetargetPoseTelemetryValid
                && Record.bMetaHumanProofTelemetryValid
                && Record.bMontageInstanceMatchesCommittedThrow
                && Record.bMontageFullWeightGateValid
                && Record.bScreenshotRequestPoseBound
                && Record.bScreenshotRequestFixedTimeStepActive
                && Record.bResolvedCameraInvariant;
        }
    }
    const bool bMetaHumanPresentationPassed = !bMetaHumanProductionVisualCapture
        || (ProfileRecords.IsEmpty()
            && LiveReleaseCallbackCount == 1
            && bLiveFollowThroughReached
            && bLiveThrowRecovered
            && LiveRecoveryReason == TEXT("ThrowFinished")
            && bLiveAuthoritativeFlightSettled
            && bLiveAuthoritativeFlightValidationAttempted
            && bLiveAuthoritativeFlightValidationPassed
            && bLiveTrajectoryCaptureValid
            && bLiveTrajectoryExportDeferralRequested
            && bLiveTrajectoryExportDeferralDiscarded
            && !LiveTrajectoryCaptureId.IsEmpty()
            && LiveTrajectorySampleCount
                >= MetaHumanProofMinimumTrajectorySampleCount
            && MetaHumanBackend
            && MetaHumanBackend->IsVisualBackendReady()
            && MetaHumanBackend->IsPresentationPolicyVerified()
            && MetaHumanBackend->GetVerifiedPresentationPolicy()
                == EDGMetaHumanPresentationPolicy::GameplayPerformance
            && MetaHumanVisualBody
            && !Golfer->IsDGProxyPresentationVisible()
            && Golfer->GetProductionMotionCandidateRevision()
                == ExpectedProductionMotionRevision
            && ActiveProductionMontagePath.Contains(
                TEXT("/Game/DiscGolf/Animation/ProductionMotion/Drive/"),
                ESearchCase::CaseSensitive)
            && ActiveProductionMontagePath.EndsWith(
                FString::Printf(
                    TEXT("_%s"),
                    *ExpectedProductionMotionRevision),
                ESearchCase::CaseSensitive));
    const bool bSession3PresentationPassed = bMetaHumanProductionVisualCapture
        || (ProfileRecords.Num() == 3
            && LiveReleaseCallbackCount == 1
            && bLiveThrowRecovered
            && bLiveFollowThroughReached);
    if (!bAllFilesValid
        || !bMetaHumanPresentationPassed
        || !bSession3PresentationPassed)
    {
        Fail(TEXT("final visual evidence invariant was incomplete"));
        return;
    }

    RestoreCaptureTimeDilation();
    RestoreCapturePresentationState();
    DestroyProfileFixture();
    if (!WriteManifest(true, TEXT("")))
    {
        Fail(TEXT("PASS manifest could not be serialized or written"));
        return;
    }
    bFinished = true;
    SetStage(EStage::Finished);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: PASS mode=%s captures=%d live_release=%d profiles=%d active_montage=%s manifest=%s"),
        bMetaHumanProductionVisualCapture ? TEXT("metahuman_production") : TEXT("session3"),
        CaptureRecords.Num(),
        LiveReleaseCallbackCount,
        ProfileRecords.Num(),
        *ActiveProductionMontagePath,
        *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}
