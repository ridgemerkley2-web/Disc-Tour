#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfBuiltInEnvironmentProvider.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBuiltInEnvironmentClearContractTest,
    "DiscGolfTour.Environment.BuiltInProvider.ClearContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBuiltInEnvironmentClearContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDGCourseEnvironmentState Requested;
    FDiscGolfBuiltInEnvironmentPresentation Resolved;
    FString Error;
    TestTrue(TEXT("Default built-in state resolves"),
        DiscGolfBuiltInEnvironment::Resolve(Requested, Resolved, Error));
    TestEqual(TEXT("Default status is exact"), Resolved.Status, FName(TEXT("BUILT_IN_CLEAR")));
    TestEqual(TEXT("Default weather remains clear"), Resolved.AppliedMode,
        EDiscGolfBuiltInEnvironmentMode::Clear);
    TestFalse(TEXT("Default state does not use fallback"), Resolved.bUsedFallback);
    TestTrue(TEXT("Default time is retained"), FMath::IsNearlyEqual(Resolved.TimeOfDayHours, 14.0f));
    TestTrue(TEXT("Default sun pitch preserves accepted presentation"),
        FMath::IsNearlyEqual(Resolved.SunRotation.Pitch, -38.0f));
    TestTrue(TEXT("Default sun yaw preserves accepted presentation"),
        FMath::IsNearlyEqual(Resolved.SunRotation.Yaw, -35.0f));
    TestTrue(TEXT("Default sun intensity is preserved"),
        FMath::IsNearlyEqual(Resolved.SunIntensity, 1.25f));
    TestTrue(TEXT("Default sky intensity is preserved"),
        FMath::IsNearlyEqual(Resolved.SkyLightIntensity, 0.72f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBuiltInEnvironmentOvercastContractTest,
    "DiscGolfTour.Environment.BuiltInProvider.OvercastContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBuiltInEnvironmentOvercastContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDGCourseEnvironmentState Requested;
    Requested.TimeOfDayHours = 9.0f;
    Requested.Weather = EDGWeatherPreset::Overcast;
    Requested.WeatherIntensity = 0.8f;
    Requested.Wetness = 0.9f;
    FDiscGolfBuiltInEnvironmentPresentation Resolved;
    FString Error;
    TestTrue(TEXT("Overcast built-in state resolves"),
        DiscGolfBuiltInEnvironment::Resolve(Requested, Resolved, Error));
    TestEqual(TEXT("Overcast status is explicit"), Resolved.Status,
        FName(TEXT("BUILT_IN_OVERCAST_LIGHTING_ONLY")));
    TestEqual(TEXT("Overcast mode is exact"), Resolved.AppliedMode,
        EDiscGolfBuiltInEnvironmentMode::Overcast);
    TestFalse(TEXT("Built-in overcast is directly supported"), Resolved.bUsedFallback);
    TestTrue(TEXT("Overcast reduces direct sun"), Resolved.SunIntensity < 1.25f);
    TestTrue(TEXT("Overcast raises ambient light"), Resolved.SkyLightIntensity > 0.72f);
    TestTrue(TEXT("Overcast raises Mie scattering"), Resolved.MieScatteringScale > 0.003996f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBuiltInEnvironmentUnsupportedWeatherTest,
    "DiscGolfTour.Environment.BuiltInProvider.UnsupportedWeatherFailsSafe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBuiltInEnvironmentUnsupportedWeatherTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDGCourseEnvironmentState Requested;
    Requested.Weather = EDGWeatherPreset::Storm;
    Requested.WeatherIntensity = 1.0f;
    Requested.Wetness = 1.0f;
    FDiscGolfBuiltInEnvironmentPresentation Resolved;
    FString Error;
    TestTrue(TEXT("Unsupported precipitation resolves to a visual fallback"),
        DiscGolfBuiltInEnvironment::Resolve(Requested, Resolved, Error));
    TestTrue(TEXT("Fallback is disclosed"), Resolved.bUsedFallback);
    TestEqual(TEXT("Fallback status forbids implied gameplay effects"), Resolved.Status,
        FName(TEXT("UNSUPPORTED_PRECIPITATION_FALLBACK_OVERCAST_NO_GAMEPLAY_EFFECTS")));
    TestEqual(TEXT("Fallback is overcast presentation"), Resolved.AppliedMode,
        EDiscGolfBuiltInEnvironmentMode::Overcast);

    Requested.TimeOfDayHours = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite time fails closed"),
        DiscGolfBuiltInEnvironment::Resolve(Requested, Resolved, Error));
    TestTrue(TEXT("Non-finite rejection is explained"), !Error.IsEmpty());
    return true;
}

#endif
