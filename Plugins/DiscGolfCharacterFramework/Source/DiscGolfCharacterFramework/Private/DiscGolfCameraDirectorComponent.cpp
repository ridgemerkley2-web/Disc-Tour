#include "DiscGolfCameraDirectorComponent.h"

UDiscGolfCameraDirectorComponent::UDiscGolfCameraDirectorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDiscGolfCameraDirectorComponent::RequestCamera(const FDGCameraRequest& Request)
{
    ActiveRequest = Request;
    OnCameraRequest.Broadcast(ActiveRequest);
    ApplyCameraRequest(ActiveRequest);
}

void UDiscGolfCameraDirectorComponent::RequestSimpleMode(
    EDGCameraMode Mode,
    AActor* PrimaryTarget,
    float BlendTimeSeconds)
{
    FDGCameraRequest Request;
    Request.Mode = Mode;
    Request.PrimaryTarget = PrimaryTarget;
    Request.BlendTimeSeconds = FMath::Max(0.0f, BlendTimeSeconds);
    RequestCamera(Request);
}
