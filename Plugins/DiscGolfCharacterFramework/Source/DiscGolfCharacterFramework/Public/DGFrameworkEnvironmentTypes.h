#pragma once

#include "CoreMinimal.h"
#include "DGFrameworkEnvironmentTypes.generated.h"

UENUM(BlueprintType)
enum class EDGWeatherPreset : uint8
{
    Clear,
    PartlyCloudy,
    Overcast,
    LightRain,
    HeavyRain,
    Storm,
    Snow
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGCourseEnvironmentState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment", meta=(ClampMin="0.0", ClampMax="24.0"))
    float TimeOfDayHours = 14.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    EDGWeatherPreset Weather = EDGWeatherPreset::Clear;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment", meta=(ClampMin="0.0", ClampMax="1.0"))
    float WeatherIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Wetness = 0.0f;
};
