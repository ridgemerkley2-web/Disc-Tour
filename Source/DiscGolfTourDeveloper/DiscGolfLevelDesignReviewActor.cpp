#include "DiscGolfLevelDesignReviewActor.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"

namespace
{
    FColor RouteColor(EDiscGolfShotRouteType Type)
    {
        switch (Type)
        {
            case EDiscGolfShotRouteType::RiskReward: return FColor(255, 176, 55);
            case EDiscGolfShotRouteType::Bailout: return FColor(95, 220, 150);
            case EDiscGolfShotRouteType::Primary:
            default: return FColor(70, 210, 235);
        }
    }

    FString RouteTypeName(EDiscGolfShotRouteType Type)
    {
        switch (Type)
        {
            case EDiscGolfShotRouteType::RiskReward: return TEXT("RISK / REWARD");
            case EDiscGolfShotRouteType::Bailout: return TEXT("BAILOUT");
            case EDiscGolfShotRouteType::Primary:
            default: return TEXT("PRIMARY");
        }
    }
}

ADiscGolfLevelDesignReviewActor::ADiscGolfLevelDesignReviewActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    Tags.AddUnique(TEXT("CourseFeature.LevelDesignReview"));
}

void ADiscGolfLevelDesignReviewActor::Configure(
    const TArray<FDiscGolfShotRouteDefinition>& InRoutes,
    const TArray<FDiscGolfLandingZoneDefinition>& InLandingZones)
{
    Routes = InRoutes;
    LandingZones = InLandingZones;
}

void ADiscGolfLevelDesignReviewActor::SetReviewVisible(bool bVisible)
{
    bReviewVisible = bVisible;
    SetActorTickEnabled(bVisible);
}

void ADiscGolfLevelDesignReviewActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bReviewVisible || !GetWorld()) return;

    constexpr float LiftCm = 45.0f;
    for (const FDiscGolfShotRouteDefinition& Route : Routes)
    {
        const bool bFocused = FocusedRouteId.IsNone() || Route.RouteId == FocusedRouteId;
        const FColor BaseColor = RouteColor(Route.RouteType);
        const FColor Color = bFocused ? BaseColor : FColor(
            BaseColor.R / 5, BaseColor.G / 5, BaseColor.B / 5);
        const float ArrowThickness = bFocused ? 10.0f : 3.0f;
        const float EdgeThickness = bFocused ? 3.0f : 1.0f;
        const float HalfWidth = Route.CorridorWidthCm * 0.5f;
        for (int32 Index = 1; Index < Route.WaypointsCm.Num(); ++Index)
        {
            const FVector Start = Route.WaypointsCm[Index - 1] + FVector(0, 0, LiftCm);
            const FVector End = Route.WaypointsCm[Index] + FVector(0, 0, LiftCm);
            const FVector FlatDirection(End.X - Start.X, End.Y - Start.Y, 0.0f);
            const FVector Side = FVector::CrossProduct(FVector::UpVector, FlatDirection.GetSafeNormal());
            DrawDebugDirectionalArrow(GetWorld(), Start, End, 120.0f, Color, false, 0.0f, 0, ArrowThickness);
            DrawDebugLine(GetWorld(), Start + Side * HalfWidth, End + Side * HalfWidth,
                Color, false, 0.0f, 0, EdgeThickness);
            DrawDebugLine(GetWorld(), Start - Side * HalfWidth, End - Side * HalfWidth,
                Color, false, 0.0f, 0, EdgeThickness);
        }

        for (int32 Index = 1; Index + 1 < Route.WaypointsCm.Num(); ++Index)
        {
            DrawDebugSphere(GetWorld(), Route.WaypointsCm[Index] + FVector(0,0,LiftCm),
                95.0f, 12, Color, false, 0.0f, 0, bFocused ? 5.0f : 1.0f);
        }
        if (Route.WaypointsCm.Num() >= 2)
        {
            const FVector LabelLocation = Route.WaypointsCm[1] + FVector(0,0,240.0f);
            const FString Label = FString::Printf(TEXT("%s // %s // TARGET %d // R%d W%d"),
                *RouteTypeName(Route.RouteType), *Route.Label.ToString(), Route.TargetStrokes,
                Route.RiskRating, Route.RewardRating);
            DrawDebugString(GetWorld(), LabelLocation, Label, nullptr, Color, 0.0f, false,
                bFocused ? 1.2f : 0.75f);
        }
    }

    for (const FDiscGolfLandingZoneDefinition& Zone : LandingZones)
    {
        const FDiscGolfShotRouteDefinition* FocusedRoute = Routes.FindByPredicate([this](const auto& Route)
        {
            return Route.RouteId == FocusedRouteId;
        });
        const bool bFocusedZone = !FocusedRoute || FocusedRoute->LandingZoneId == Zone.ZoneId;
        const FColor ZoneColor = bFocusedZone ? FColor(220, 245, 255) : FColor(44, 49, 51);
        const FVector Center = Zone.LocationCm + FVector(0,0,LiftCm);
        DrawDebugBox(GetWorld(), Center, FVector(Zone.ExtentCm.X, Zone.ExtentCm.Y, 25.0f),
            Zone.Rotation.Quaternion(), ZoneColor, false, 0.0f, 0, bFocusedZone ? 5.0f : 1.0f);
        DrawDebugString(GetWorld(), Center + FVector(0,0,160.0f), Zone.Label.ToString(),
            nullptr, ZoneColor, 0.0f, false, bFocusedZone ? 1.1f : 0.7f);
    }
}

bool ADiscGolfLevelDesignReviewActor::IsCollisionInvariant() const
{
    return FindComponentByClass<UPrimitiveComponent>() == nullptr;
}
