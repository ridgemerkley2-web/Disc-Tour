#include "DiscReplayActor.h"

#include "DiscGolfPresentationMath.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

ADiscReplayActor::ADiscReplayActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(false);

    GhostDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReplayGhostDisc"));
    SetRootComponent(GhostDisc);
    GhostDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GhostDisc->SetCastShadow(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        GhostDisc->SetStaticMesh(CylinderMesh.Object);
        GhostDisc->SetRelativeScale3D(FVector(0.21f, 0.21f, 0.015f));
    }

    CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ReplayCameraArm"));
    CameraArm->SetupAttachment(GhostDisc);
    CameraArm->TargetArmLength = 360.0f;
    CameraArm->SocketOffset = FVector(0.0f, 75.0f, 45.0f);
    CameraArm->SetRelativeRotation(FRotator(-9.0f, 0.0f, 0.0f));
    CameraArm->bDoCollisionTest = false;
    CameraArm->bEnableCameraLag = true;
    CameraArm->CameraLagSpeed = 7.0f;

    ReplayCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ReplayCamera"));
    ReplayCamera->SetupAttachment(CameraArm);

    TeeCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TeeReplayCamera"));
    TeeCamera->SetupAttachment(GhostDisc);
    TeeCamera->SetUsingAbsoluteLocation(true);
    TeeCamera->SetUsingAbsoluteRotation(true);
    TeeCamera->SetFieldOfView(54.0f);
    TeeCamera->SetActive(false);

    GhostLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ReplayGhostLight"));
    GhostLight->SetupAttachment(GhostDisc);
    GhostLight->SetRelativeLocation(FVector(0.0f, 0.0f, 12.0f));
    GhostLight->SetLightColor(FLinearColor(1.0f, 0.62f, 0.08f));
    GhostLight->SetIntensity(900.0f);
    GhostLight->SetAttenuationRadius(180.0f);
    GhostLight->SetCastShadows(false);
}

void ADiscReplayActor::BeginPlay()
{
    Super::BeginPlay();
    if (UMaterialInstanceDynamic* GhostMaterial = GhostDisc->CreateAndSetMaterialInstanceDynamic(0))
    {
        GhostMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.48f, 0.04f));
    }
}

bool ADiscReplayActor::InitializeReplay(
    const TArray<FDiscTrajectorySample>& InSamples,
    float InPlaybackRate)
{
    Samples.Reset();
    PlaybackTimeSeconds = 0.0f;
    DurationSeconds = 0.0f;
    FinishHoldRemaining = 0.0f;
    bPlaying = false;
    bPaused = false;
    bFinishBroadcast = false;
    LastValidationError.Reset();
    SetActorTickEnabled(false);
    SetActorHiddenInGame(true);

    if (!ValidateReplayInput(InSamples, LastValidationError)) return false;
    if (!FMath::IsFinite(InPlaybackRate))
    {
        LastValidationError = TEXT("Replay playback rate must be finite");
        return false;
    }

    Samples = InSamples;
    DurationSeconds = Samples.Last().TimeSeconds - Samples[0].TimeSeconds;
    PlaybackRate = NormalizePlaybackRate(InPlaybackRate);
    bPlaying = true;
    CameraMode = EDiscReplayCameraMode::Tracking;
    ReplayCamera->SetActive(true);
    TeeCamera->SetActive(false);
    ApplyPlaybackFrame(0.0f);
    const FVector Launch = Samples[0].WorldLocationCm;
    const FVector Direction = (Samples[1].WorldLocationCm - Launch).GetSafeNormal2D(
        SMALL_NUMBER, FVector::ForwardVector);
    const FVector LookAt = Samples[FMath::Min(Samples.Num() - 1, FMath::Max(1, Samples.Num() / 5))].WorldLocationCm;
    const FVector TeeView = Launch - Direction * 720.0f + FVector(0.0f, 0.0f, 250.0f);
    TeeCamera->SetWorldLocation(TeeView);
    TeeCamera->SetWorldRotation((LookAt - TeeView).Rotation());
    SetActorHiddenInGame(!bPlaying);
    SetActorTickEnabled(bPlaying);
    return bPlaying;
}

bool ADiscReplayActor::ValidateReplayInput(
    const TArray<FDiscTrajectorySample>& InSamples,
    FString& OutError)
{
    for (int32 Index = 1; Index < InSamples.Num(); ++Index)
    {
        if (!FMath::IsFinite(InSamples[Index - 1].TimeSeconds)
            || !FMath::IsFinite(InSamples[Index].TimeSeconds)
            || InSamples[Index].TimeSeconds <= InSamples[Index - 1].TimeSeconds)
        {
            OutError = TEXT("Replay actor samples must be strictly time ordered");
            return false;
        }
    }
    FDiscActualReplaySelectionPolicy Policy;
    Policy.MaxSourceSamples = MaxReplayInputSamples;
    Policy.MaxOutputSamples = MaxReplayInputSamples;
    Policy.MaxSampleRateHz = 240.0f;
    Policy.MaxDurationSeconds = MaxReplayDurationSeconds;
    return DiscGolfPresentationMath::ValidateActualReplaySource(
        InSamples, TArray<FDiscGroundTransition>(), Policy, OutError);
}

float ADiscReplayActor::NormalizePlaybackRate(float InPlaybackRate)
{
    return FMath::IsFinite(InPlaybackRate)
        ? FMath::Clamp(InPlaybackRate, MinimumPlaybackRate, MaximumPlaybackRate)
        : 0.75f;
}

float ADiscReplayActor::NextPlaybackRate(float InPlaybackRate)
{
    const float Rate = NormalizePlaybackRate(InPlaybackRate);
    if (Rate < 0.50f - KINDA_SMALL_NUMBER) return 0.50f;
    if (Rate < 0.75f - KINDA_SMALL_NUMBER) return 0.75f;
    if (Rate < 1.00f - KINDA_SMALL_NUMBER) return 1.00f;
    if (Rate < 1.50f - KINDA_SMALL_NUMBER) return 1.50f;
    return 0.50f;
}

void ADiscReplayActor::CycleCameraMode()
{
    CameraMode = CameraMode == EDiscReplayCameraMode::Tracking
        ? EDiscReplayCameraMode::Tee : EDiscReplayCameraMode::Tracking;
    ReplayCamera->SetActive(CameraMode == EDiscReplayCameraMode::Tracking);
    TeeCamera->SetActive(CameraMode == EDiscReplayCameraMode::Tee);
}

void ADiscReplayActor::PauseReplay()
{
    if (!bPlaying || bFinishBroadcast) return;
    bPlaying = false;
    bPaused = true;
    SetActorTickEnabled(false);
}

bool ADiscReplayActor::ResumeReplay()
{
    if (Samples.Num() < 2 || bFinishBroadcast || bPlaying) return false;
    bPaused = false;
    bPlaying = true;
    if (PlaybackTimeSeconds >= DurationSeconds && FinishHoldRemaining <= 0.0f)
    {
        FinishHoldRemaining = 0.65f;
    }
    SetActorTickEnabled(true);
    return true;
}

bool ADiscReplayActor::SeekReplay(float TimeSeconds)
{
    if (Samples.Num() < 2 || bFinishBroadcast || !FMath::IsFinite(TimeSeconds)) return false;
    PlaybackTimeSeconds = FMath::Clamp(TimeSeconds, 0.0f, DurationSeconds);
    FinishHoldRemaining = PlaybackTimeSeconds >= DurationSeconds ? 0.65f : 0.0f;
    ApplyPlaybackFrame(PlaybackTimeSeconds);
    return true;
}

bool ADiscReplayActor::SetPlaybackRate(float InPlaybackRate)
{
    if (!FMath::IsFinite(InPlaybackRate)) return false;
    PlaybackRate = NormalizePlaybackRate(InPlaybackRate);
    return true;
}

float ADiscReplayActor::CyclePlaybackRate()
{
    PlaybackRate = NextPlaybackRate(PlaybackRate);
    return PlaybackRate;
}

void ADiscReplayActor::SetReducedMotion(bool bInReducedMotion)
{
    bReducedMotion = bInReducedMotion;
    if (CameraArm)
    {
        CameraArm->bEnableCameraLag = !bReducedMotion;
    }
}

void ADiscReplayActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPlaying) return;

    if (FinishHoldRemaining > 0.0f)
    {
        FinishHoldRemaining -= DeltaSeconds;
        if (FinishHoldRemaining <= 0.0f) FinishReplay();
        return;
    }

    PlaybackTimeSeconds = FMath::Min(PlaybackTimeSeconds + FMath::Max(DeltaSeconds, 0.0f) * PlaybackRate, DurationSeconds);
    ApplyPlaybackFrame(PlaybackTimeSeconds);
    if (PlaybackTimeSeconds >= DurationSeconds)
    {
        FinishHoldRemaining = 0.65f;
    }
}

void ADiscReplayActor::ApplyPlaybackFrame(float TimeSeconds)
{
    if (!DiscGolfPresentationMath::EvaluateReplayFrame(Samples, TimeSeconds, CurrentFrame)) return;
    SetActorLocationAndRotation(
        CurrentFrame.WorldLocationCm,
        CurrentFrame.WorldRotation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
}

float ADiscReplayActor::GetProgress01() const
{
    return DurationSeconds > SMALL_NUMBER
        ? FMath::Clamp(PlaybackTimeSeconds / DurationSeconds, 0.0f, 1.0f)
        : 0.0f;
}

void ADiscReplayActor::CancelReplay()
{
    if (bFinishBroadcast) return;
    FinishReplay();
}

void ADiscReplayActor::FinishReplay()
{
    if (bFinishBroadcast) return;
    bFinishBroadcast = true;
    bPlaying = false;
    bPaused = false;
    SetActorTickEnabled(false);
    OnReplayFinished.Broadcast(this);
}
