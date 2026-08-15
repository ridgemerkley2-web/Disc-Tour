#include "DiscGolfEnvironmentBridgeComponent.h"

UDiscGolfEnvironmentBridgeComponent::UDiscGolfEnvironmentBridgeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDiscGolfEnvironmentBridgeComponent::ApplyEnvironmentState(
    const FDGCourseEnvironmentState& NewState)
{
    CurrentState = NewState;
    CurrentState.TimeOfDayHours = FMath::Clamp(CurrentState.TimeOfDayHours, 0.0f, 24.0f);
    CurrentState.WeatherIntensity = FMath::Clamp(CurrentState.WeatherIntensity, 0.0f, 1.0f);
    CurrentState.Wetness = FMath::Clamp(CurrentState.Wetness, 0.0f, 1.0f);
    ApplyEnvironmentToProvider(CurrentState);
}
