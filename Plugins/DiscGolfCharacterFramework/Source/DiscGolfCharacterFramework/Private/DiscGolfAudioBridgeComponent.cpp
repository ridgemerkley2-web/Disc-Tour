#include "DiscGolfAudioBridgeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UDiscGolfAudioBridgeComponent::UDiscGolfAudioBridgeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDiscGolfAudioBridgeComponent::EmitAudioEvent(const FDGAudioEventPayload& Payload)
{
    if (bPlayFallbackSounds)
    {
        for (const FDGAudioFallbackEntry& Entry : FallbackSounds)
        {
            if (Entry.EventType == Payload.EventType)
            {
                if (USoundBase* Sound = Entry.Sound.LoadSynchronous())
                {
                    UGameplayStatics::PlaySoundAtLocation(
                        this,
                        Sound,
                        Payload.WorldLocationCm,
                        FMath::Clamp(Payload.Intensity01, 0.0f, 1.0f)
                    );
                }
                break;
            }
        }
    }

    RouteAudioEventToProvider(Payload);
}
