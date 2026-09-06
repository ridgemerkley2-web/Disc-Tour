#pragma once

#include "CoreMinimal.h"
#include "DGFrameworkEnvironmentTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfBuiltInEnvironmentProvider.generated.h"

class UDirectionalLightComponent;
class USceneComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

UENUM(BlueprintType)
enum class EDiscGolfBuiltInEnvironmentMode : uint8
{
    Clear,
    Overcast
};

/**
 * Resolved visual-only state for the built-in Unreal environment provider.
 * It intentionally contains no wind, lie, collision, traction, or scoring data.
 */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfBuiltInEnvironmentPresentation
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float TimeOfDayHours = 14.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    EDGWeatherPreset RequestedWeather = EDGWeatherPreset::Clear;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    EDiscGolfBuiltInEnvironmentMode AppliedMode = EDiscGolfBuiltInEnvironmentMode::Clear;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    bool bUsedFallback = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    FName Status = TEXT("BUILT_IN_CLEAR");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    FRotator SunRotation = FRotator(-38.0f, -35.0f, 0.0f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float SunIntensity = 1.25f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    FLinearColor SunColor = FLinearColor(1.0f, 0.98f, 0.94f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float SkyLightIntensity = 0.72f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float RayleighScatteringScale = 0.0331f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float MieScatteringScale = 0.003996f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    float MieAbsorptionScale = 0.000444f;
};

namespace DiscGolfBuiltInEnvironment
{
    /** Pure, deterministic resolver. Unsupported precipitation falls back visually only. */
    DISCGOLFTOUR_API bool Resolve(
        const FDGCourseEnvironmentState& RequestedState,
        FDiscGolfBuiltInEnvironmentPresentation& OutPresentation,
        FString& OutError);
}

/**
 * Built-in Unreal lighting/atmosphere provider for cleared development content.
 * Applying a state only changes this actor's presentation components.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfBuiltInEnvironmentProvider : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfBuiltInEnvironmentProvider();

    bool ApplyEnvironmentState(
        const FDGCourseEnvironmentState& RequestedState,
        FString& OutError);

    const FDiscGolfBuiltInEnvironmentPresentation& GetPresentation() const
    {
        return Presentation;
    }

    UFUNCTION(BlueprintPure, Category="Environment")
    FName GetEnvironmentStatus() const { return Presentation.Status; }

    UFUNCTION(BlueprintPure, Category="Environment")
    bool IsUsingFallback() const { return Presentation.bUsedFallback; }

    UFUNCTION(BlueprintPure, Category="Environment")
    bool IsGameplayNeutral() const { return true; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    TObjectPtr<UDirectionalLightComponent> Sun;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Environment")
    TObjectPtr<USkyLightComponent> SkyLight;

private:
    UPROPERTY(VisibleAnywhere, Category="Environment")
    FDiscGolfBuiltInEnvironmentPresentation Presentation;
};
