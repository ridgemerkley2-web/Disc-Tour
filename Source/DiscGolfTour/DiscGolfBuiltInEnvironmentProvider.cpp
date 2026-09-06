#include "DiscGolfBuiltInEnvironmentProvider.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"

namespace
{
    constexpr float DefaultTimeOfDayHours = 14.0f;
    constexpr float ClearSunIntensity = 1.25f;
    constexpr float ClearSkyIntensity = 0.72f;
    constexpr float EarthRayleighScatteringScale = 0.0331f;
    constexpr float EarthMieScatteringScale = 0.003996f;
    constexpr float EarthMieAbsorptionScale = 0.000444f;

    FRotator ResolveSunRotation(const float TimeOfDayHours)
    {
        const float DistanceFromNoon = FMath::Abs(TimeOfDayHours - 12.0f);
        const float AltitudeDegrees = FMath::Clamp(
            70.0f - DistanceFromNoon * 16.0f,
            2.0f,
            70.0f);
        const float YawDegrees = FMath::UnwindDegrees(
            -35.0f + (TimeOfDayHours - DefaultTimeOfDayHours) * 15.0f);
        return FRotator(-AltitudeDegrees, YawDegrees, 0.0f);
    }
}

bool DiscGolfBuiltInEnvironment::Resolve(
    const FDGCourseEnvironmentState& RequestedState,
    FDiscGolfBuiltInEnvironmentPresentation& OutPresentation,
    FString& OutError)
{
    OutError.Reset();
    if (!FMath::IsFinite(RequestedState.TimeOfDayHours)
        || !FMath::IsFinite(RequestedState.WeatherIntensity)
        || !FMath::IsFinite(RequestedState.Wetness))
    {
        OutError = TEXT("built-in environment state contains a non-finite numeric value");
        return false;
    }

    FDiscGolfBuiltInEnvironmentPresentation Resolved;
    Resolved.TimeOfDayHours = FMath::Clamp(RequestedState.TimeOfDayHours, 0.0f, 24.0f);
    Resolved.RequestedWeather = RequestedState.Weather;
    Resolved.SunRotation = ResolveSunRotation(Resolved.TimeOfDayHours);
    Resolved.RayleighScatteringScale = EarthRayleighScatteringScale;
    Resolved.MieAbsorptionScale = EarthMieAbsorptionScale;

    switch (RequestedState.Weather)
    {
        case EDGWeatherPreset::Clear:
            Resolved.AppliedMode = EDiscGolfBuiltInEnvironmentMode::Clear;
            Resolved.Status = TEXT("BUILT_IN_CLEAR");
            Resolved.SunIntensity = ClearSunIntensity;
            Resolved.SunColor = FLinearColor(1.0f, 0.98f, 0.94f);
            Resolved.SkyLightIntensity = ClearSkyIntensity;
            Resolved.MieScatteringScale = EarthMieScatteringScale;
            break;

        case EDGWeatherPreset::Overcast:
            Resolved.AppliedMode = EDiscGolfBuiltInEnvironmentMode::Overcast;
            Resolved.Status = TEXT("BUILT_IN_OVERCAST_LIGHTING_ONLY");
            Resolved.SunIntensity = 0.55f;
            Resolved.SunColor = FLinearColor(0.82f, 0.87f, 0.95f);
            Resolved.SkyLightIntensity = 0.95f;
            Resolved.MieScatteringScale = 0.02f;
            break;

        case EDGWeatherPreset::PartlyCloudy:
            Resolved.AppliedMode = EDiscGolfBuiltInEnvironmentMode::Clear;
            Resolved.bUsedFallback = true;
            Resolved.Status = TEXT("UNSUPPORTED_PARTLY_CLOUDY_FALLBACK_CLEAR");
            Resolved.SunIntensity = ClearSunIntensity;
            Resolved.SunColor = FLinearColor(1.0f, 0.98f, 0.94f);
            Resolved.SkyLightIntensity = ClearSkyIntensity;
            Resolved.MieScatteringScale = EarthMieScatteringScale;
            break;

        case EDGWeatherPreset::LightRain:
        case EDGWeatherPreset::HeavyRain:
        case EDGWeatherPreset::Storm:
        case EDGWeatherPreset::Snow:
        default:
            Resolved.AppliedMode = EDiscGolfBuiltInEnvironmentMode::Overcast;
            Resolved.bUsedFallback = true;
            Resolved.Status = TEXT("UNSUPPORTED_PRECIPITATION_FALLBACK_OVERCAST_NO_GAMEPLAY_EFFECTS");
            Resolved.SunIntensity = 0.55f;
            Resolved.SunColor = FLinearColor(0.82f, 0.87f, 0.95f);
            Resolved.SkyLightIntensity = 0.95f;
            Resolved.MieScatteringScale = 0.02f;
            break;
    }

    OutPresentation = Resolved;
    return true;
}

ADiscGolfBuiltInEnvironmentProvider::ADiscGolfBuiltInEnvironmentProvider()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
    Sun->SetupAttachment(Root);
    Sun->SetMobility(EComponentMobility::Movable);
    Sun->SetAtmosphereSunLight(true);

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(Root);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(Root);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->SetRealTimeCapture(false);

    FDGCourseEnvironmentState DefaultState;
    FString Error;
    ensureAlwaysMsgf(ApplyEnvironmentState(DefaultState, Error),
        TEXT("Built-in environment default state failed: %s"), *Error);

    Tags.AddUnique(TEXT("Environment.Provider.BuiltIn"));
    Tags.AddUnique(TEXT("Environment.PresentationOnly"));
}

bool ADiscGolfBuiltInEnvironmentProvider::ApplyEnvironmentState(
    const FDGCourseEnvironmentState& RequestedState,
    FString& OutError)
{
    FDiscGolfBuiltInEnvironmentPresentation Resolved;
    if (!DiscGolfBuiltInEnvironment::Resolve(RequestedState, Resolved, OutError))
    {
        return false;
    }

    Presentation = Resolved;
    SetActorRotation(Presentation.SunRotation);
    Sun->SetIntensity(Presentation.SunIntensity);
    Sun->SetLightColor(Presentation.SunColor);
    SkyLight->SetIntensity(Presentation.SkyLightIntensity);
    SkyAtmosphere->SetRayleighScatteringScale(Presentation.RayleighScatteringScale);
    SkyAtmosphere->SetMieScatteringScale(Presentation.MieScatteringScale);
    SkyAtmosphere->SetMieAbsorptionScale(Presentation.MieAbsorptionScale);

    if (GetWorld())
    {
        SkyLight->RecaptureSky();
    }
    return true;
}
