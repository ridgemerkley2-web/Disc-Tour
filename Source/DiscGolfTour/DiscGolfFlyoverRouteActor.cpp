#include "DiscGolfFlyoverRouteActor.h"

#include "Camera/CameraComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/PlayerController.h"

ADiscGolfFlyoverRouteActor::ADiscGolfFlyoverRouteActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(false);
    RouteSpline = CreateDefaultSubobject<USplineComponent>(TEXT("FlyoverSpline"));
    SetRootComponent(RouteSpline);
    PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FlyoverCamera"));
    PreviewCamera->SetupAttachment(RouteSpline);
    PreviewCamera->SetFieldOfView(62.0f);
    Tags.AddUnique(TEXT("CourseFeature.Flyover"));
}

void ADiscGolfFlyoverRouteActor::Configure(const TArray<FVector>& WorldPointsCm, const FVector& InLookAtWorldCm)
{
    // The spline owns authored world-space points and must remain stationary while
    // the attached camera travels along it.
    SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
    RouteSpline->ClearSplinePoints(false);
    for (const FVector& Point : WorldPointsCm)
    {
        RouteSpline->AddSplinePoint(Point, ESplineCoordinateSpace::World, false);
    }
    RouteSpline->SetClosedLoop(false, false);
    RouteSpline->UpdateSpline();
    LookAtWorldCm = InLookAtWorldCm;
}

bool ADiscGolfFlyoverRouteActor::StartPreview(APlayerController* PlayerController)
{
    if (!PlayerController || GetPointCount() < 2 || GetRouteLengthCm() <= 0.0f) return false;
    PreviewController = PlayerController; PreviewElapsedSeconds = 0.0f; bPreviewing = true;
    SetActorTickEnabled(true);
    const FVector Start = RouteSpline->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World);
    PreviewCamera->SetWorldLocation(Start);
    PreviewCamera->SetWorldRotation((LookAtWorldCm - Start).Rotation());
    PlayerController->SetViewTargetWithBlend(this, 0.35f);
    return true;
}

void ADiscGolfFlyoverRouteActor::StopPreview()
{
    if (!bPreviewing) return;
    bPreviewing = false; SetActorTickEnabled(false); PreviewController = nullptr;
}

void ADiscGolfFlyoverRouteActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPreviewing) return;
    PreviewElapsedSeconds += DeltaSeconds;
    const float Alpha = FMath::Clamp(PreviewElapsedSeconds / FMath::Max(PreviewDurationSeconds, 0.1f), 0.0f, 1.0f);
    const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
    const FVector Location = RouteSpline->GetLocationAtDistanceAlongSpline(
        GetRouteLengthCm() * SmoothAlpha, ESplineCoordinateSpace::World);
    PreviewCamera->SetWorldLocation(Location);
    PreviewCamera->SetWorldRotation((LookAtWorldCm - Location + FVector(0,0,120)).Rotation());
    if (Alpha >= 1.0f)
    {
        StopPreview(); OnFlyoverFinished.Broadcast(this);
    }
}

int32 ADiscGolfFlyoverRouteActor::GetPointCount() const
{
    return RouteSpline ? RouteSpline->GetNumberOfSplinePoints() : 0;
}

float ADiscGolfFlyoverRouteActor::GetRouteLengthCm() const
{
    return RouteSpline ? RouteSpline->GetSplineLength() : 0.0f;
}
