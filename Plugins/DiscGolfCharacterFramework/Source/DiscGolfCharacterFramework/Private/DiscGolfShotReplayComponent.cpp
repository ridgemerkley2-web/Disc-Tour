#include "DiscGolfShotReplayComponent.h"

UDiscGolfShotReplayComponent::UDiscGolfShotReplayComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UDiscGolfShotReplayComponent::LoadRecord(const FDGThrowTelemetryRecord& Record)
{
    StopPlayback();
    LoadedRecord = Record;
    PlaybackTimeSeconds = 0.0f;
    return LoadedRecord.Samples.Num() > 0;
}

void UDiscGolfShotReplayComponent::PlayFromStart(float InPlaybackRate)
{
    if (LoadedRecord.Samples.Num() <= 0)
    {
        return;
    }

    PlaybackRate = FMath::Max(0.01f, InPlaybackRate);
    PlaybackTimeSeconds = 0.0f;
    bPlaying = true;
    SetComponentTickEnabled(true);
    EmitCurrentFrame(false);
}

void UDiscGolfShotReplayComponent::PausePlayback()
{
    bPlaying = false;
    SetComponentTickEnabled(false);
}

void UDiscGolfShotReplayComponent::StopPlayback()
{
    bPlaying = false;
    PlaybackTimeSeconds = 0.0f;
    SetComponentTickEnabled(false);
}

void UDiscGolfShotReplayComponent::Seek(float TimeSeconds)
{
    const float EndTime = LoadedRecord.Samples.Num() > 0
        ? LoadedRecord.Samples.Last().TimeSeconds
        : 0.0f;

    PlaybackTimeSeconds = FMath::Clamp(TimeSeconds, 0.0f, EndTime);
    EmitCurrentFrame(PlaybackTimeSeconds >= EndTime);
}

void UDiscGolfShotReplayComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bPlaying || LoadedRecord.Samples.Num() <= 0)
    {
        return;
    }

    PlaybackTimeSeconds += DeltaTime * PlaybackRate;

    const float EndTime = LoadedRecord.Samples.Last().TimeSeconds;
    const bool bFinished = PlaybackTimeSeconds >= EndTime;

    if (bFinished)
    {
        PlaybackTimeSeconds = EndTime;
    }

    EmitCurrentFrame(bFinished);

    if (bFinished)
    {
        bPlaying = false;
        SetComponentTickEnabled(false);
    }
}

void UDiscGolfShotReplayComponent::EmitCurrentFrame(bool bForceFinished)
{
    if (LoadedRecord.Samples.Num() <= 0)
    {
        return;
    }

    const TArray<FDGFlightSample>& Samples = LoadedRecord.Samples;

    int32 NextIndex = 0;
    while (NextIndex < Samples.Num() && Samples[NextIndex].TimeSeconds < PlaybackTimeSeconds)
    {
        ++NextIndex;
    }

    const int32 BIndex = FMath::Clamp(NextIndex, 0, Samples.Num() - 1);
    const int32 AIndex = FMath::Max(0, BIndex - 1);

    const FDGFlightSample& A = Samples[AIndex];
    const FDGFlightSample& B = Samples[BIndex];

    float Alpha = 0.0f;
    const float Span = B.TimeSeconds - A.TimeSeconds;
    if (Span > KINDA_SMALL_NUMBER)
    {
        Alpha = FMath::Clamp((PlaybackTimeSeconds - A.TimeSeconds) / Span, 0.0f, 1.0f);
    }

    FDGShotReplayFrame Frame;
    Frame.PlaybackTimeSeconds = PlaybackTimeSeconds;
    Frame.WorldLocationCm = FMath::Lerp(A.WorldLocationCm, B.WorldLocationCm, Alpha);
    Frame.Orientation = FQuat::Slerp(
        A.Orientation.Quaternion(),
        B.Orientation.Quaternion(),
        Alpha
    ).Rotator();
    Frame.LinearVelocityMps = FMath::Lerp(A.LinearVelocityMps, B.LinearVelocityMps, Alpha);
    Frame.bFinished = bForceFinished;

    OnShotReplayFrame.Broadcast(Frame);
}
