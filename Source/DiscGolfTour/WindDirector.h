#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindDirector.generated.h"

class ADiscGolfWindZoneActor;

/** Immutable value snapshot of one ordered local modifier used by physics wind. */
struct DISCGOLFTOUR_API FDiscGolfPhysicsWindZoneSnapshot
{
    TWeakObjectPtr<ADiscGolfWindZoneActor> ZoneActor;
    FName ZoneId = NAME_None;
    float BaseWindScale = 1.0f;
    FVector AdditiveWindMps = FVector::ZeroVector;
    FBox WorldBounds = FBox(ForceInit);

    bool Equals(const FDiscGolfPhysicsWindZoneSnapshot& Other) const;
};

/** Complete ordered wind state frozen by an authoritative disc launch. */
struct DISCGOLFTOUR_API FDiscGolfPhysicsWindConfigurationSnapshot
{
    FVector BaseWindMps = FVector::ZeroVector;
    float GustAmplitudeMps = 0.0f;
    float GustFrequencyHz = 0.0f;
    TArray<FDiscGolfPhysicsWindZoneSnapshot> Zones;
    bool bCaptured = false;

    bool Equals(const FDiscGolfPhysicsWindConfigurationSnapshot& Other) const;
};

UCLASS()
class DISCGOLFTOUR_API AWindDirector : public AActor
{
    GENERATED_BODY()

public:
    AWindDirector();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind") FVector BaseWindMps = FVector(2.5f, 0.8f, 0.0f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.0", ClampMax="15.0"))
    float GustAmplitudeMps = 1.4f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.01", ClampMax="2.0"))
    float GustFrequencyHz = 0.12f;

    /**
     * Atomic runtime configuration seam. Invalid values leave the current wind
     * untouched so a transient authoring/runtime fault cannot partially update
     * the physics environment.
     */
    UFUNCTION(BlueprintCallable, Category="Wind")
    bool TryConfigurePhysicsWind(
        const FVector& InBaseWindMps,
        float InGustAmplitudeMps,
        float InGustFrequencyHz,
        FString& OutError);

    /** Validates the complete currently sampled physics environment, including active zone modifiers. */
    bool ValidatePhysicsWindConfiguration(FString& OutError) const;

    /** Atomically captures every ordered value that can affect fixed-step wind sampling. */
    bool TryCapturePhysicsWindConfiguration(
        FDiscGolfPhysicsWindConfigurationSnapshot& OutSnapshot,
        FString& OutError) const;

    /** Fails closed when live physics wind no longer equals an accepted launch snapshot. */
    bool MatchesPhysicsWindConfiguration(
        const FDiscGolfPhysicsWindConfigurationSnapshot& ExpectedSnapshot,
        FString& OutError) const;

    /**
     * Produces a replayable phase origin from the one-based accepted-shot
     * sequence. Physics must never derive this value from the render clock.
     */
    static bool TryBuildDeterministicShotPhaseOrigin(
        int32 AcceptedShotSequence,
        float& OutPhaseOriginSeconds,
        FString& OutError);

    /** Presentation-time sample retained for HUD, foliage and other visual consumers. */
    UFUNCTION(BlueprintPure, Category="Wind") FVector GetWindMpsAt(const FVector& WorldLocation) const;

    /**
     * Deterministic physics sample at an explicit solver-owned time. Disc flight
     * must use this seam instead of inheriting the render-ticked presentation clock.
     */
    UFUNCTION(BlueprintPure, Category="Wind")
    FVector GetWindMpsAtSimulationTime(
        const FVector& WorldLocation,
        float SimulationTimeSeconds) const;

    /** Fail-closed physics sampling seam used by authoritative fixed-step flight. */
    bool TryGetWindMpsAtSimulationTime(
        const FVector& WorldLocation,
        float SimulationTimeSeconds,
        FVector& OutWindMps,
        FString& OutError) const;
    UFUNCTION(BlueprintCallable, Category="Wind") void RefreshCourseZones();
    UFUNCTION(BlueprintPure, Category="Wind") FName GetActiveZoneIdAt(const FVector& WorldLocation) const;
    UFUNCTION(BlueprintPure, Category="Wind") int32 GetCourseZoneCount() const { return CourseZones.Num(); }

private:
    bool ValidatePhysicsWindZones(FString& OutError) const;
    float SimTime = 0.0f;
    UPROPERTY() TArray<TWeakObjectPtr<ADiscGolfWindZoneActor>> CourseZones;
};
