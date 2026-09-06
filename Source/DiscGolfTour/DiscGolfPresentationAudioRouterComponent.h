#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfAudioTypes.h"
#include "DiscGolfPresentationAudio.h"
#include "DiscGolfPresentationAudioRouterComponent.generated.h"

struct FStreamableHandle;
class USoundBase;

/** Optional project-owned sound binding. An empty table is a valid silent configuration. */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfPresentationAudioFallbackBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation Audio")
    FName EventId = NAME_None;

    /** Optional category fallback used only when no exact semantic EventId is authored. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation Audio")
    FName CategoryId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation Audio")
    TSoftObjectPtr<USoundBase> Sound;
};

/**
 * Sanitized presentation request. FDGAudioEventPayload is a value-only compatibility DTO;
 * the semantic project EventId and pitch remain available so a provider never has to guess.
 */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfPresentationAudioRoute
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Presentation Audio")
    FName EventId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Presentation Audio")
    FDGAudioEventPayload Payload;

    UPROPERTY(BlueprintReadOnly, Category="Presentation Audio")
    float PitchMultiplier = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category="Presentation Audio")
    float ListenerDistanceCm = 0.0f;

    /** Project-owned normalized settings gain; kept separate from semantic intensity. */
    UPROPERTY(BlueprintReadOnly, Category="Presentation Audio")
    float OutputGain01 = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category="Presentation Audio")
    bool bReplayPresentation = false;

    UPROPERTY(BlueprintReadOnly, Category="Presentation Audio")
    bool bFallbackSoundReady = false;

    bool IsValid() const;
};

/**
 * No-tick, presentation-only adapter from validated project semantic events to audio providers.
 * It cannot launch, simulate, score, save, or mutate gameplay. Missing assets are a valid,
 * explicitly reported silent route rather than a runtime failure.
 */
UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UDiscGolfPresentationAudioRouterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfPresentationAudioRouterComponent();

    static constexpr int32 MaximumRouteTrace = 64;
    static constexpr float MaximumSpeedMps = 100.0f;
    static constexpr float MaximumSpinRpm = 5000.0f;
    static constexpr float MaximumWobbleDegrees = 90.0f;
    static constexpr float MaximumListenerDistanceCm = 10000000.0f;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /**
     * Builds one finite provider request. Optional kinematics are read-only modulation values;
     * invalid values sanitize to zero and never invalidate an otherwise valid semantic event.
     */
    static bool BuildRoute(
        const FDiscGolfPresentationAudioEvent& Event,
        FDiscGolfPresentationAudioRoute& OutRoute,
        float SpeedMps = 0.0f,
        float SpinRpm = 0.0f,
        float WobbleDegrees = 0.0f,
        float Wetness01 = 0.0f,
        float ListenerDistanceCm = 0.0f,
        float OutputGain01 = 1.0f);

    /** Returns true when a new validated route was accepted, including a silent no-asset route. */
    bool ConsumeSemanticEvent(
        const FDiscGolfPresentationAudioEvent& Event,
        float SpeedMps = 0.0f,
        float SpinRpm = 0.0f,
        float WobbleDegrees = 0.0f,
        float Wetness01 = 0.0f,
        float ListenerDistanceCm = 0.0f,
        float OutputGain01 = 1.0f);

    UFUNCTION(BlueprintCallable, Category="Presentation Audio")
    void PrimeFallbackAssets();

    UFUNCTION(BlueprintPure, Category="Presentation Audio")
    int32 GetRouteTraceCount() const { return RouteTrace.Num(); }

    UFUNCTION(BlueprintPure, Category="Presentation Audio")
    int32 GetGeneratedCandidateBindingCount() const
    {
        return GeneratedCandidateFallbackSounds.Num();
    }

    bool HasGeneratedCandidateBinding(EDiscGolfPresentationAudioCategory Category) const;

    UFUNCTION(BlueprintPure, Category="Presentation Audio")
    FDiscGolfPresentationAudioRoute GetLastRoute() const
    {
        return RouteTrace.IsEmpty() ? FDiscGolfPresentationAudioRoute() : RouteTrace.Last();
    }

    UFUNCTION(BlueprintPure, Category="Presentation Audio")
    FString GetRouterStatusText() const { return RouterStatusText; }

    UFUNCTION(BlueprintCallable, Category="Presentation Audio")
    void ResetRouteTrace();

    /** Authored Blueprint/MetaSound providers consume the sanitized request, never gameplay state. */
    UFUNCTION(BlueprintImplementableEvent, Category="Presentation Audio", meta=(DisplayName="Route Presentation Audio Event"))
    void RoutePresentationAudioEvent(const FDiscGolfPresentationAudioRoute& Route);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation Audio")
    bool bEnableFallbackPlayback = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation Audio")
    TArray<FDiscGolfPresentationAudioFallbackBinding> FallbackSounds;

private:
    const FDiscGolfPresentationAudioFallbackBinding* FindFallback(
        const FDiscGolfPresentationAudioEvent& Event) const;

    UPROPERTY(Transient)
    TArray<FDiscGolfPresentationAudioRoute> RouteTrace;

    /** Project-owned generated candidates; explicit cook coverage is configured separately. */
    UPROPERTY(Transient)
    TArray<FDiscGolfPresentationAudioFallbackBinding> GeneratedCandidateFallbackSounds;

    FString LastSemanticDedupeKey;
    FString RouterStatusText = TEXT("AUDIO ROUTER READY - SILENT ASSET FALLBACK");
    TSharedPtr<FStreamableHandle> FallbackPreloadHandle;
};
