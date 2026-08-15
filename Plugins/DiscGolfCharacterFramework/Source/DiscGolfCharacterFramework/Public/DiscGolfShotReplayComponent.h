#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfTelemetryTypes.h"
#include "DiscGolfShotReplayComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGOnShotReplayFrame,
    FDGShotReplayFrame,
    Frame
);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfShotReplayComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfShotReplayComponent();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Replay")
    float PlaybackRate = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Replay")
    bool bPlaying = false;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Replay")
    float PlaybackTimeSeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Replay")
    FDGThrowTelemetryRecord LoadedRecord;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Replay")
    FDGOnShotReplayFrame OnShotReplayFrame;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Replay")
    bool LoadRecord(const FDGThrowTelemetryRecord& Record);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Replay")
    void PlayFromStart(float InPlaybackRate = 1.0f);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Replay")
    void PausePlayback();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Replay")
    void StopPlayback();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Replay")
    void Seek(float TimeSeconds);

private:
    void EmitCurrentFrame(bool bForceFinished);
};
