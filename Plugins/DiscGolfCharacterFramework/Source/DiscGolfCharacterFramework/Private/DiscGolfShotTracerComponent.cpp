#include "DiscGolfShotTracerComponent.h"
#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"

UDiscGolfShotTracerComponent::UDiscGolfShotTracerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDiscGolfShotTracerComponent::BeginPlay()
{
    Super::BeginPlay();
    EnsureSpline();
}

void UDiscGolfShotTracerComponent::EnsureSpline()
{
    if (TracerSpline || !GetOwner())
    {
        return;
    }

    TracerSpline = NewObject<USplineComponent>(
        GetOwner(),
        TEXT("DG_TracerSpline"),
        RF_Transient
    );

    if (USceneComponent* Root = GetOwner()->GetRootComponent())
    {
        TracerSpline->SetupAttachment(Root);
    }

    TracerSpline->RegisterComponent();
}

void UDiscGolfShotTracerComponent::BeginTracer(FVector ReleaseLocationCm)
{
    EnsureSpline();
    WorldPoints.Reset();
    WorldPoints.Add(ReleaseLocationCm);

    if (TracerSpline)
    {
        TracerSpline->ClearSplinePoints(false);
        TracerSpline->AddSplinePoint(
            ReleaseLocationCm,
            ESplineCoordinateSpace::World,
            true
        );
    }

    OnTracerUpdated.Broadcast(WorldPoints);
    RebuildTracerVisual(WorldPoints, TracerColor, bVisible);
}

bool UDiscGolfShotTracerComponent::AddTracerPoint(FVector WorldLocationCm)
{
    if (WorldPoints.Num() <= 0)
    {
        BeginTracer(WorldLocationCm);
        return true;
    }

    if (WorldPoints.Num() >= FMath::Max(2, MaximumPoints))
    {
        return false;
    }

    if (FVector::DistSquared(WorldPoints.Last(), WorldLocationCm) <
        FMath::Square(FMath::Max(0.0f, MinimumPointDistanceCm)))
    {
        return false;
    }

    WorldPoints.Add(WorldLocationCm);

    EnsureSpline();
    if (TracerSpline)
    {
        TracerSpline->AddSplinePoint(
            WorldLocationCm,
            ESplineCoordinateSpace::World,
            true
        );
    }

    OnTracerUpdated.Broadcast(WorldPoints);
    RebuildTracerVisual(WorldPoints, TracerColor, bVisible);
    return true;
}

void UDiscGolfShotTracerComponent::EndTracer()
{
    OnTracerUpdated.Broadcast(WorldPoints);
    RebuildTracerVisual(WorldPoints, TracerColor, bVisible);
}

void UDiscGolfShotTracerComponent::ClearTracer()
{
    WorldPoints.Reset();

    if (TracerSpline)
    {
        TracerSpline->ClearSplinePoints(true);
    }

    OnTracerUpdated.Broadcast(WorldPoints);
    RebuildTracerVisual(WorldPoints, TracerColor, false);
}
