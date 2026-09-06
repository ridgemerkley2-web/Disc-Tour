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
struct DISCGOLFRUNTIMEFOUNDATION_API FDGCourseEnvironmentState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TimeOfDayHours = 14.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGWeatherPreset Weather = EDGWeatherPreset::Clear;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WeatherIntensity = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Wetness = 0.0f;
};
