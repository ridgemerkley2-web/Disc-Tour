#include "DiscGolfSession3VisualCaptureRunner.h"

#include "DiscActor.h"
#include "DiscBagComponent.h"
#include "DiscGolferPawn.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfThrowComponent.h"
#include "DiscFlightComponent.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "ThrowControllerComponent.h"
#include "DrawDebugHelpers.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace DiscGolfSession3VisualCapture
{
constexpr double ReadinessTimeoutSeconds = 8.0;
constexpr double TimingCaptureSeconds = 0.12;
constexpr double ScreenshotTimeoutSeconds = 30.0;
constexpr double ReleaseTimeoutSeconds = 5.0;
constexpr double GameplayRecoveryTimeoutSeconds = 65.0;
constexpr double ProfileTimeoutSeconds = 6.0;
// Frame 54 is the validated reachback fixture. The grip is clear of the torso
// here, so the real attached Cylinder reads cleanly before release.
constexpr float HeldCaptureMontageSeconds = 0.90f;
constexpr float FollowThroughCaptureMontageSeconds = 1.86f;

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

bool IsFiniteVector(const FVector& Value)
{
    return !Value.ContainsNaN()
        && FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
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

    GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    ThrowAdapter = Golfer ? Golfer->GetRHBHThrowAdapter() : nullptr;
    GolferMesh = FindSkeletalMesh(Golfer);
    HeldDiscVisual = FindNamedStaticMesh(Golfer, TEXT("HeldDiscVisual"));
    ThrowMontage = LoadObject<UAnimMontage>(
        nullptr,
        TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype.AM_DG_RHBH_Prototype"));

    if (!GameMode || !Golfer || !ThrowAdapter || !GolferMesh || !HeldDiscVisual
        || !ThrowMontage || !Golfer->GetThrowController() || !Golfer->GetDiscBag())
    {
        Fail(TEXT("gameplay pawn, montage, adapter, held disc, throw controller, or disc bag was unavailable"));
        return;
    }

    OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("CharacterFramework/Screenshots/Session3_FirstThrow"));
    ManifestPath = FPaths::Combine(OutputDirectory, TEXT("Session3_FirstThrow_CaptureManifest.json"));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    for (const TCHAR* Filename : CaptureFilenames)
    {
        IFileManager::Get().Delete(*FPaths::Combine(OutputDirectory, Filename), false, true, true);
    }
    IFileManager::Get().Delete(*ManifestPath, false, true, true);

    GameMode->SkipCurrentPresentation();
    InitialGolferTransform = Golfer->GetActorTransform();
    BaselineWorldDiscCount = CountWorldDiscs();
    BaselineStrokes = GameMode->GetStrokes();
    BaselineLiveReleaseCount = ThrowAdapter->GetTotalReleaseCommitCount();
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

    const auto CreateCaptureLight = [this](
        const TCHAR* Name,
        const FVector& RelativeLocation,
        float IntensityLumens,
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
        Light->SetIntensity(IntensityLumens);
        Light->SetAttenuationRadius(2500.0f);
        Light->SetLightColor(Color);
        Light->SetCastShadows(false);
        Light->SetVolumetricScatteringIntensity(0.0f);
        Light->RegisterComponent();
        return Light;
    };
    // Capture-only sports-photography lighting. These transient components do
    // not save into the level or alter gameplay/environment light authority.
    CaptureKeyLight = CreateCaptureLight(
        TEXT("Session3CaptureKey"), FVector(220.0f, -180.0f, 220.0f),
        4000.0f, FColor(255, 244, 226));
    CaptureFillLight = CreateCaptureLight(
        TEXT("Session3CaptureFill"), FVector(140.0f, 220.0f, 80.0f),
        1200.0f, FColor(205, 224, 255));
    if (!CaptureKeyLight || !CaptureFillLight)
    {
        Fail(TEXT("transient capture key/fill lights could not be created"));
        return;
    }
    ApplyNeutralProxyMaterials(GolferMesh);

    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetViewTarget(CaptureCamera);
        Controller->ConsoleCommand(TEXT("r.MotionBlurQuality 0"), true);
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
        TEXT("DG_SESSION3_VISUAL_CAPTURE: START output=%s"), *OutputDirectory);
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
            if (GameMode->CanPlayerThrow())
            {
                FThrowCommand Ignored;
                if (!BuildTimingCommand(Ignored, true))
                {
                    Fail(TEXT("existing throw controller did not enter timing capture"));
                    return;
                }
                SetStage(EStage::BuildingLiveCommand);
            }
            else if (SecondsInStage() > ReadinessTimeoutSeconds)
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
            if (GetMontagePosition(GolferMesh) >= HeldCaptureMontageSeconds)
            {
                PrepareHeldCapture();
            }
            else if (SecondsInStage() > ReleaseTimeoutSeconds)
            {
                Fail(TEXT("held-disc montage pose was not reached"));
            }
            break;

        case EStage::WaitingForRelease:
            if (SecondsInStage() > ReleaseTimeoutSeconds)
            {
                Fail(TEXT("DG Release Disc did not fire during visual capture"));
            }
            break;

        case EStage::WaitingForPostRelease:
            if (SecondsInStage() >= 0.10)
            {
                PreparePostReleaseCapture();
            }
            break;

        case EStage::WaitingForFollowThrough:
            if (GetMontagePosition(GolferMesh) >= FollowThroughCaptureMontageSeconds)
            {
                bLiveFollowThroughReached = ThrowAdapter->IsThrowActive();
                PrepareFollowThroughCapture();
            }
            else if (SecondsInStage() > ReleaseTimeoutSeconds)
            {
                Fail(TEXT("follow-through montage pose was not reached"));
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
            else if (SecondsInStage() > GameplayRecoveryTimeoutSeconds)
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
        case EStage::CapturingExactRelease:
        case EStage::CapturingPostRelease:
        case EStage::CapturingFollowThrough:
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
    if (ThrowAdapter)
    {
        ThrowAdapter->OnReleaseCommitted.RemoveDynamic(
            this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveRelease);
        ThrowAdapter->OnThrowRecovered.RemoveDynamic(
            this, &ADiscGolfSession3VisualCaptureRunner::HandleLiveRecovery);
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
        Golfer->GetActorForwardVector(), OutCommand);
}

void ADiscGolfSession3VisualCaptureRunner::BeginLiveThrow()
{
    if (!BuildTimingCommand(LiveCommand, false)
        || LiveCommand.ThrowStyle != EThrowStyle::Backhand
        || LiveCommand.ShotContext != EDiscShotContext::Drive
        || !Golfer->TryStartAnimatedRHBHThrow(LiveCommand))
    {
        Fail(TEXT("real RHBH command did not enter the project animation adapter"));
        return;
    }
    SetStage(EStage::WaitingForHeldPose);
}

void ADiscGolfSession3VisualCaptureRunner::PrepareHeldCapture()
{
    if (!HeldDiscVisual->IsVisible() || HeldDiscVisual->bHiddenInGame
        || !ThrowAdapter->IsAwaitingRelease() || CountWorldDiscs() != BaselineWorldDiscCount)
    {
        Fail(TEXT("held-disc capture did not have one visible grip disc and zero launched discs"));
        return;
    }
    if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
    {
        Anim->Montage_Pause(ThrowMontage);
    }
    HeldDiscVisual->UpdateBounds();
    const FVector HeldDiscLocation = GetDiscEvidenceLocation(HeldDiscVisual);
    PositionGripCloseupCamera(
        Golfer, GolferMesh, HeldDiscLocation, 36.0f);
    ClearEvidenceMarkers();
    PendingEvidenceMesh = HeldDiscVisual;
    PendingEvidenceMarkerColor = FColor::Cyan;
    DrawDiscEvidenceMarker(HeldDiscLocation, PendingEvidenceMarkerColor);
    ShowEvidenceLabel(
        TEXT("SESSION 3 | HELD DISC BEFORE RELEASE"),
        TEXT("CYAN RING: HELD CYLINDER AT disc_grip_r  |  GAMEPLAY DISCS: 0"));
    SetStage(EStage::CapturingHeld);
    RequestCapture(0, TEXT("Held provisional Cylinder follows disc_grip_r before release."),
        TEXT("possessed gameplay pawn + live montage"));
    ValidateCameraFraming(
        0, Golfer, GolferMesh, HeldDiscLocation, false,
        GetProvisionalDiscRadiusCm(HeldDiscVisual), HeldDiscVisual);
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

    UStaticMeshComponent* ReleasedDiscMesh = FindNamedStaticMesh(
        LiveGameplayDisc, TEXT("DiscMesh"));
    if (ReleasedDiscMesh)
    {
        GameplayDiscEvidenceMaterial = ApplyDiscEvidenceMaterial(
            ReleasedDiscMesh, FLinearColor(1.0f, 0.16f, 0.01f, 1.0f));
    }
    if (!ReleasedDiscMesh || !GameplayDiscEvidenceMaterial)
    {
        Fail(TEXT("capture-only gameplay-disc material could not be created"));
        return;
    }

    // Evidence must follow the rendered Cylinder component, not merely the actor
    // transform. Keeping this explicit also catches any future visual-offset layer.
    ReleasedDiscMesh->UpdateBounds();
    const FVector ReleasedDiscLocation = GetDiscEvidenceLocation(ReleasedDiscMesh);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: release mesh_alignment_cm=%.6f actor=(%s) mesh=(%s)"),
        FVector::Dist(LiveGameplayDisc->GetActorLocation(), ReleasedDiscLocation),
        *LiveGameplayDisc->GetActorLocation().ToCompactString(),
        *ReleasedDiscLocation.ToCompactString());

    if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
    {
        Anim->Montage_Pause(ThrowMontage);
    }
    SetLiveDiscSimulationPaused(true);
    UGameplayStatics::SetGlobalTimeDilation(this, 0.01f);
    PositionGripCloseupCamera(
        Golfer, GolferMesh, ReleasedDiscLocation, 36.0f);
    ClearEvidenceMarkers();
    PendingEvidenceMesh = ReleasedDiscMesh;
    PendingEvidenceMarkerColor = FColor(255, 96, 16);
    DrawDiscEvidenceMarker(ReleasedDiscLocation, PendingEvidenceMarkerColor);
    ShowEvidenceLabel(
        TEXT("EXACT DG RELEASE DISC CALLBACK"),
        FString::Printf(TEXT("ORANGE RING: AUTHORITATIVE DISC  |  ATTEMPT %lld  |  HELD: 0  |  DISC: 1"), AttemptSerial));
    SetStage(EStage::CapturingExactRelease);
    RequestCapture(1, TEXT("Captured from the single DG Release Disc adapter callback."),
        TEXT("possessed gameplay pawn + authoritative gameplay disc"));
    ValidateCameraFraming(
        1, Golfer, GolferMesh, ReleasedDiscLocation, false,
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
    if (!ThrowAdapter->IsThrowActive() || LiveReleaseCallbackCount != 1)
    {
        Fail(TEXT("follow-through capture no longer had an active post-release animation"));
        return;
    }
    if (UAnimInstance* Anim = GolferMesh->GetAnimInstance())
    {
        Anim->Montage_Pause(ThrowMontage);
    }
    PendingEvidenceMesh = nullptr;
    PendingEvidenceMarkerColor = FColor::Transparent;
    ClearEvidenceMarkers();
    PositionFullBodyCamera(Golfer, GolferMesh, 40.0f);
    ShowEvidenceLabel(
        TEXT("RHBH FOLLOW-THROUGH"),
        TEXT("RELEASE COUNT: 1  |  THROW TRANSACTION STILL ACTIVE"));
    SetStage(EStage::CapturingFollowThrough);
    RequestCapture(3, TEXT("Character completes the authored RHBH follow-through after one release."),
        TEXT("possessed gameplay pawn + live montage"));
    ValidateCameraFraming(
        3, Golfer, GolferMesh, GolferMesh->GetSocketLocation(TEXT("disc_grip_r")), true);
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
    const bool bStartedProfile = ProfileFixture->TryStartAnimatedRHBHThrow(LiveCommand);
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
        && IsFiniteVector(GripWorldTransform.GetLocation())
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
    if (bScreenshotPending || CaptureIndex < 0 || CaptureIndex >= UE_ARRAY_COUNT(CaptureFilenames))
    {
        Fail(TEXT("capture request was invalid or overlapped another screenshot"));
        return;
    }
    PendingCaptureIndex = CaptureIndex;
    PendingCapturePath = FPaths::Combine(OutputDirectory, CaptureFilenames[CaptureIndex]);
    IFileManager::Get().Delete(*PendingCapturePath, false, true, true);
    bScreenshotPending = true;
    bScreenshotIssued = false;

    FCaptureRecord Record;
    Record.Filename = CaptureFilenames[CaptureIndex];
    Record.Evidence = Evidence;
    Record.Source = Source;
    CaptureRecords.Add(Record);

    if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
    {
        Controller->SetViewTarget(CaptureCamera);
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: FRAMED %d/8 %s"),
        CaptureIndex + 1, *PendingCapturePath);
}

bool ADiscGolfSession3VisualCaptureRunner::PollPendingCapture()
{
    // SetViewTarget was applied when the pose was framed. Delay the screenshot
    // request until the next world tick so PlayerCameraManager has evaluated the
    // external camera instead of reusing the previous gameplay view cache.
    if (!bScreenshotIssued)
    {
        bScreenshotIssued = true;
        PendingCaptureStartRealSeconds = FPlatformTime::Seconds();
        FScreenshotRequest::RequestScreenshot(
            PendingCapturePath,
            true,
            false,
            false,
            FIntRect(),
            true);
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION3_VISUAL_CAPTURE: REQUEST %d/8 %s"),
            PendingCaptureIndex + 1, *PendingCapturePath);
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
        if (Record.Width < 1280 || Record.Height < 720)
        {
            Fail(FString::Printf(TEXT("capture resolution was below 1280x720: %dx%d"), Record.Width, Record.Height));
            return false;
        }

        const int32 CompletedIndex = PendingCaptureIndex;
        bScreenshotPending = false;
        bScreenshotIssued = false;
        PendingCaptureIndex = INDEX_NONE;
        PendingCapturePath.Reset();
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION3_VISUAL_CAPTURE: CAPTURED %d/8 %dx%d bytes=%lld"),
            CompletedIndex + 1, Record.Width, Record.Height, Record.Bytes);
        HandleCaptureCompleted(CompletedIndex);
        return true;
    }
    if (FPlatformTime::Seconds() - PendingCaptureStartRealSeconds > ScreenshotTimeoutSeconds)
    {
        Fail(FString::Printf(TEXT("screenshot timed out: %s"), *PendingCapturePath));
    }
    return false;
}

void ADiscGolfSession3VisualCaptureRunner::HandleCaptureCompleted(int32 CaptureIndex)
{
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
    const FVector HandL = Mesh->GetSocketLocation(TEXT("hand_l"));
    const FVector HandR = Mesh->GetSocketLocation(TEXT("hand_r"));
    const bool bUpperBodyFramed = IsPointFramed(Pelvis, 0.86f)
        && IsPointFramed(Head, 0.86f)
        && IsPointFramed(HandR, 0.90f);
    const bool bFullBodyFramed = bUpperBodyFramed
        && IsPointFramed(FootL, 0.88f)
        && IsPointFramed(FootR, 0.88f)
        && IsPointFramed(HandL, 0.92f);

    const FVector EvidenceLocation = EvidenceMesh
        ? GetDiscEvidenceLocation(EvidenceMesh)
        : FocusPoint;
    Record.CameraToSubjectCm = FVector::Dist(CameraLocation, Mesh->Bounds.Origin);
    Record.SubjectBoundsRadiusCm = Mesh->Bounds.SphereRadius;
    Record.bCameraOutsideSubjectBounds = !Mesh->Bounds.GetBox().ExpandBy(35.0f).IsInside(CameraLocation);
    Record.bSubjectFramed = bRequireFullBody ? bFullBodyFramed : bUpperBodyFramed;
    Record.bFocusPointFramed = IsPointFramed(EvidenceLocation, 0.82f);
    Record.bLineOfSightClear = bLastCameraLineOfSightClear;
    Record.bBoneLengthsInvariant = ValidateBoneLengthInvariant(
        Mesh, Record.MaxBoneLengthRatioError);
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
        TEXT("DG_SESSION3_VISUAL_CAMERA: shot=%d camera=%s subject=%s radius=%.1f distance=%.1f outside=%d subject_framed=%d focus_framed=%d los=%d bone_lengths=%d max_ratio_error=%.4f screen_height=%.3f readable_scale=%d focus_diameter_px=%.1f evidence_readable=%d evidence_component=%s actor_component_cm=%.4f focus_alignment_cm=%.4f component_anchored=%d"),
        CaptureIndex + 1,
        *CameraLocation.ToCompactString(),
        *Mesh->Bounds.Origin.ToCompactString(),
        Record.SubjectBoundsRadiusCm,
        Record.CameraToSubjectCm,
        Record.bCameraOutsideSubjectBounds ? 1 : 0,
        Record.bSubjectFramed ? 1 : 0,
        Record.bFocusPointFramed ? 1 : 0,
        Record.bLineOfSightClear ? 1 : 0,
        Record.bBoneLengthsInvariant ? 1 : 0,
        Record.MaxBoneLengthRatioError,
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
    if (!GEngine)
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
    return Anim && ThrowMontage ? Anim->Montage_GetPosition(ThrowMontage) : -1.0f;
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
    return IsFiniteVector(Pelvis) && IsFiniteVector(Head) && IsFiniteVector(Hand)
        && IsFiniteVector(FootL) && IsFiniteVector(FootR)
        && IsFiniteVector(Bounds.BoxExtent)
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
    float& OutMaxRatioError) const
{
    OutMaxRatioError = 0.0f;
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
        TEXT("pelvis"),
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
        if (!IsFiniteVector(ChildLocation) || !IsFiniteVector(ParentLocation))
        {
            return false;
        }
        const float ActualLength = FVector::Dist(ChildLocation, ParentLocation);
        const float RatioError = FMath::Abs(ActualLength / ExpectedLength - 1.0f);
        OutMaxRatioError = FMath::Max(OutMaxRatioError, RatioError);
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
    return IsFiniteVector(RenderedOrigin)
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
    if (GetWorld())
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

void ADiscGolfSession3VisualCaptureRunner::WriteManifest(bool bPassed, const FString& Error)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    Root->SetStringField(TEXT("scope"), TEXT("SESSION_3_FIRST_THROW_VISUAL_EVIDENCE_ONLY"));
    Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("output_directory"), OutputDirectory);
    Root->SetNumberField(TEXT("capture_count"), CaptureRecords.Num());
    Root->SetStringField(TEXT("error"), Error);

    TArray<TSharedPtr<FJsonValue>> Captures;
    for (const FCaptureRecord& Record : CaptureRecords)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
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
        Object->SetBoolField(TEXT("bone_lengths_invariant"), Record.bBoneLengthsInvariant);
        Object->SetNumberField(TEXT("max_bone_length_ratio_error"), Record.MaxBoneLengthRatioError);
        Object->SetNumberField(TEXT("subject_screen_height_fraction"), Record.SubjectScreenHeightFraction);
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
        Captures.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("captures"), Captures);

    TSharedRef<FJsonObject> Live = MakeShared<FJsonObject>();
    Live->SetStringField(TEXT("pawn"), TEXT("possessed ADiscGolferPawn"));
    Live->SetStringField(TEXT("montage"), TEXT("/Game/DiscGolf/Animation/Throws/AM_DG_RHBH_Prototype"));
    Live->SetStringField(TEXT("release_source"), TEXT("DG Release Disc -> project adapter -> RequestThrowFromGrip"));
    Live->SetNumberField(TEXT("release_callback_count"), LiveReleaseCallbackCount);
    Live->SetNumberField(TEXT("stroke_delta"), GameMode ? GameMode->GetStrokes() - BaselineStrokes : -1);
    Live->SetBoolField(TEXT("follow_through_reached"), bLiveFollowThroughReached);
    Live->SetBoolField(TEXT("recovered"), bLiveThrowRecovered);
    Live->SetStringField(TEXT("recovery_reason"), LiveRecoveryReason);
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
    Writes->SetBoolField(TEXT("png_and_manifest_only"), true);
    Writes->SetArrayField(TEXT("uasset_writes"), TArray<TSharedPtr<FJsonValue>>());
    Writes->SetArrayField(TEXT("level_save_calls"), TArray<TSharedPtr<FJsonValue>>());
    Writes->SetArrayField(TEXT("content_save_calls"), TArray<TSharedPtr<FJsonValue>>());
    Root->SetObjectField(TEXT("writes"), Writes);
    Root->SetStringField(TEXT("production_limitation"),
        TEXT("Provisional validation proxy and gameplay Cylinder; final deformation/hand ergonomics remain deferred."));

    FString Json;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    FFileHelper::SaveStringToFile(Json, *ManifestPath);
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
    WriteManifest(false, Reason);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: FAIL %s | captures=%d manifest=%s"),
        *Reason, CaptureRecords.Num(), *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession3VisualCaptureRunner::Pass()
{
    if (bFinished)
    {
        return;
    }
    const bool bAllCapturesPresent = CaptureRecords.Num() == 8;
    bool bAllFilesValid = bAllCapturesPresent;
    for (const FCaptureRecord& Record : CaptureRecords)
    {
        bAllFilesValid &= Record.Bytes > 4096 && Record.Width >= 1280 && Record.Height >= 720
            && Record.bCameraOutsideSubjectBounds
            && Record.bSubjectFramed
            && Record.bFocusPointFramed
            && Record.bLineOfSightClear
            && Record.bBoneLengthsInvariant
            && Record.bSubjectReadableScale
            && Record.bEvidenceMeshReadable;
    }
    if (!bAllFilesValid || ProfileRecords.Num() != 3
        || LiveReleaseCallbackCount != 1 || !bLiveThrowRecovered || !bLiveFollowThroughReached)
    {
        Fail(TEXT("final visual evidence invariant was incomplete"));
        return;
    }

    bFinished = true;
    SetStage(EStage::Finished);
    RestoreCaptureTimeDilation();
    DestroyProfileFixture();
    WriteManifest(true, TEXT(""));
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION3_VISUAL_CAPTURE: PASS captures=8 live_release=1 profiles=3 manifest=%s"),
        *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}
