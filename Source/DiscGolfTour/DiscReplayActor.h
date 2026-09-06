#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfPresentationMath.h"
#include "DiscReplayActor.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ADiscReplayActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiscReplayFinishedSignature, ADiscReplayActor*, ReplayActor);

UENUM(BlueprintType)
enum class EDiscReplayCameraMode : uint8
{
    Tracking,
    Tee
};

/** Presentation-only ghost that follows recorded trajectory samples without resimulating a shot. */
UCLASS()
class DISCGOLFTOUR_API ADiscReplayActor : public AActor
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxReplayInputSamples = 2400;
    static constexpr float MaxReplayDurationSeconds = 120.0f;
    static constexpr float MinimumPlaybackRate = 0.10f;
    static constexpr float MaximumPlaybackRate = 2.0f;

    ADiscReplayActor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(BlueprintAssignable) FDiscReplayFinishedSignature OnReplayFinished;

    bool InitializeReplay(const TArray<FDiscTrajectorySample>& InSamples, float InPlaybackRate = 0.75f);
    void CancelReplay();
    UFUNCTION(BlueprintCallable) void CycleCameraMode();
    UFUNCTION(BlueprintCallable) void PauseReplay();
    UFUNCTION(BlueprintCallable) bool ResumeReplay();
    UFUNCTION(BlueprintCallable) bool SeekReplay(float TimeSeconds);
    UFUNCTION(BlueprintCallable) bool SetPlaybackRate(float InPlaybackRate);
    UFUNCTION(BlueprintCallable) float CyclePlaybackRate();
    UFUNCTION(BlueprintCallable) void SetReducedMotion(bool bInReducedMotion);

    static bool ValidateReplayInput(
        const TArray<FDiscTrajectorySample>& InSamples,
        FString& OutError);
    static float NormalizePlaybackRate(float InPlaybackRate);
    static float NextPlaybackRate(float InPlaybackRate);

    UFUNCTION(BlueprintPure) bool IsReplayPlaying() const { return bPlaying; }
    UFUNCTION(BlueprintPure) bool IsReplayPaused() const { return bPaused; }
    UFUNCTION(BlueprintPure) bool IsReducedMotion() const { return bReducedMotion; }
    UFUNCTION(BlueprintPure) float GetPlaybackTimeSeconds() const { return PlaybackTimeSeconds; }
    UFUNCTION(BlueprintPure) float GetDurationSeconds() const { return DurationSeconds; }
    UFUNCTION(BlueprintPure) float GetPlaybackRate() const { return PlaybackRate; }
    UFUNCTION(BlueprintPure) float GetProgress01() const;
    UFUNCTION(BlueprintPure) EDiscReplayCameraMode GetCameraMode() const { return CameraMode; }
    UFUNCTION(BlueprintPure) FString GetLastValidationError() const { return LastValidationError; }
    const FDiscReplayFrame& GetCurrentFrame() const { return CurrentFrame; }

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GhostDisc;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraArm;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ReplayCamera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> TeeCamera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> GhostLight;

    TArray<FDiscTrajectorySample> Samples;
    FDiscReplayFrame CurrentFrame;
    float PlaybackTimeSeconds = 0.0f;
    float DurationSeconds = 0.0f;
    float PlaybackRate = 0.75f;
    float FinishHoldRemaining = 0.0f;
    bool bPlaying = false;
    bool bPaused = false;
    bool bReducedMotion = false;
    bool bFinishBroadcast = false;
    FString LastValidationError;
    EDiscReplayCameraMode CameraMode = EDiscReplayCameraMode::Tracking;

    void ApplyPlaybackFrame(float TimeSeconds);
    void FinishReplay();
};
