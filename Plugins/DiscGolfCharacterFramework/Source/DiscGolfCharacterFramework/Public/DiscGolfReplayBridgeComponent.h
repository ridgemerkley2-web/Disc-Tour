#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCameraTypes.h"
#include "DiscGolfReplayBridgeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FDGOnReplayStatusChanged,
    EDGReplayMode, Mode,
    bool, bActive
);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfReplayBridgeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfReplayBridgeComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Replay")
    EDGReplayMode PreferredMode = EDGReplayMode::TelemetryShotReplay;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Replay")
    FDGOnReplayStatusChanged OnReplayStatusChanged;

    // Compile this bridge against the project's installed UE version. Replay API overloads can evolve;
    // telemetry replay remains the shipping-safe fallback.
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Replay")
    bool StartEngineReplayRecording(
        FString ReplayName,
        FString FriendlyName,
        const TArray<FString>& AdditionalOptions
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Replay")
    bool StopEngineReplayRecording();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Replay")
    bool PlayEngineReplay(
        FString ReplayName,
        const TArray<FString>& AdditionalOptions
    );
};
