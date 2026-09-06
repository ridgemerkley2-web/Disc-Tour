#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseDefinition.h"
#include "GameFramework/Actor.h"
#include "DiscGolfLevelDesignReviewActor.generated.h"

class USceneComponent;

/** Opt-in designer view of authored scoring corridors. It has no gameplay primitives or collision. */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfLevelDesignReviewActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfLevelDesignReviewActor();
    virtual void Tick(float DeltaSeconds) override;

    void Configure(
        const TArray<FDiscGolfShotRouteDefinition>& InRoutes,
        const TArray<FDiscGolfLandingZoneDefinition>& InLandingZones);
    void SetReviewVisible(bool bVisible);
    void SetFocusedRoute(FName RouteId) { FocusedRouteId = RouteId; }

    UFUNCTION(BlueprintPure, Category="Course|Level Design")
    bool IsReviewVisible() const { return bReviewVisible; }

    UFUNCTION(BlueprintPure, Category="Course|Level Design")
    int32 GetRouteCount() const { return Routes.Num(); }

    UFUNCTION(BlueprintPure, Category="Course|Level Design")
    bool IsCollisionInvariant() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TArray<FDiscGolfShotRouteDefinition> Routes;

    UPROPERTY()
    TArray<FDiscGolfLandingZoneDefinition> LandingZones;

    bool bReviewVisible = false;
    FName FocusedRouteId = NAME_None;
};
