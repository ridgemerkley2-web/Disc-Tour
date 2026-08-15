#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfAudioTypes.h"
#include "DiscGolfAudioBridgeComponent.generated.h"

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfAudioBridgeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfAudioBridgeComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Audio")
    bool bPlayFallbackSounds = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Audio")
    TArray<FDGAudioFallbackEntry> FallbackSounds;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Audio")
    void EmitAudioEvent(const FDGAudioEventPayload& Payload);

    // Implement with authored MetaSound Sources/Presets when available.
    UFUNCTION(BlueprintImplementableEvent, Category="Disc Golf|Audio")
    void RouteAudioEventToProvider(const FDGAudioEventPayload& Payload);
};
