#include "DiscGolfCourseFeatureActor.h"

#include "Components/StaticMeshComponent.h"

ADiscGolfCourseFeatureActor::ADiscGolfCourseFeatureActor()
{
    PrimaryActorTick.bCanEverTick = false;
    GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetStaticMeshComponent()->SetCastShadow(false);
}

void ADiscGolfCourseFeatureActor::ConfigureLandingZone(const FDiscGolfLandingZoneDefinition& D)
{
    FeatureId = D.ZoneId; FeatureType = EDiscGolfCourseFeatureType::LandingZone; Label = D.Label;
    SetActorLocationAndRotation(D.LocationCm, D.Rotation);
    SetActorScale3D(FVector(D.ExtentCm.X / 50.0f, D.ExtentCm.Y / 50.0f, 0.025f));
    // Landing-zone intent is presented by the opt-in route review overlay, not production scenery.
    GetStaticMeshComponent()->SetVisibility(false);
    Tags.AddUnique(TEXT("CourseFeature.LandingZone"));
}

void ADiscGolfCourseFeatureActor::ConfigureCameraAnchor(const FDiscGolfCameraAnchorDefinition& D)
{
    FeatureId = D.AnchorId; FeatureType = EDiscGolfCourseFeatureType::CameraAnchor;
    CameraMode = D.Mode; CameraFieldOfViewDeg = D.FieldOfViewDeg;
    SetActorLocation(D.LocationCm); SetActorScale3D(FVector(0.32f, 0.32f, 1.6f));
    GetStaticMeshComponent()->SetVisibility(false);
    Tags.AddUnique(TEXT("CourseFeature.CameraAnchor"));
}

void ADiscGolfCourseFeatureActor::ConfigureSpectatorBoundary(const FDiscGolfSpectatorBoundaryDefinition& D)
{
    FeatureId = D.BoundaryId; FeatureType = EDiscGolfCourseFeatureType::SpectatorBoundary;
    SetActorLocationAndRotation(D.LocationCm, D.Rotation);
    SetActorScale3D(FVector(D.ExtentCm.X / 50.0f, D.ExtentCm.Y / 50.0f, D.ExtentCm.Z / 50.0f));
    // Spectator boundaries are authored metadata until crowd dressing exists.
    GetStaticMeshComponent()->SetVisibility(false);
    Tags.AddUnique(TEXT("CourseFeature.SpectatorBoundary"));
}
