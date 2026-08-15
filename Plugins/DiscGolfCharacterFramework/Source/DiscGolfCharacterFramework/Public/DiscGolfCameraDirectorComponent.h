#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCameraTypes.h"
#include "DiscGolfCameraDirectorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGOnCameraRequest,
    FDGCameraRequest,
    Request
);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCameraDirectorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfCameraDirectorComponent();

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Camera")
    FDGCameraRequest ActiveRequest;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Camera")
    FDGOnCameraRequest OnCameraRequest;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Camera")
    void RequestCamera(const FDGCameraRequest& Request);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Camera")
    void RequestSimpleMode(
        EDGCameraMode Mode,
        AActor* PrimaryTarget,
        float BlendTimeSeconds
    );

    // Project-specific implementation may use PlayerCameraManager, Blueprint cameras,
    // or the optional Gameplay Cameras plugin.
    UFUNCTION(BlueprintImplementableEvent, Category="Disc Golf|Camera")
    void ApplyCameraRequest(const FDGCameraRequest& Request);
};
