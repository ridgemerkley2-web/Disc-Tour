#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfCourseFeatureActor.generated.h"

UENUM(BlueprintType)
enum class EDiscGolfCourseFeatureType : uint8
{
    LandingZone,
    CameraAnchor,
    SpectatorBoundary
};

/** Non-authoritative course metadata/visual marker used by blockout and camera systems. */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfCourseFeatureActor : public AStaticMeshActor
{
    GENERATED_BODY()

public:
    ADiscGolfCourseFeatureActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course Feature") FName FeatureId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course Feature") EDiscGolfCourseFeatureType FeatureType = EDiscGolfCourseFeatureType::LandingZone;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course Feature") FText Label;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course Feature") EDiscGolfCameraAnchorMode CameraMode = EDiscGolfCameraAnchorMode::Launch;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course Feature") float CameraFieldOfViewDeg = 60.0f;

    void ConfigureLandingZone(const FDiscGolfLandingZoneDefinition& Definition);
    void ConfigureCameraAnchor(const FDiscGolfCameraAnchorDefinition& Definition);
    void ConfigureSpectatorBoundary(const FDiscGolfSpectatorBoundaryDefinition& Definition);
};
