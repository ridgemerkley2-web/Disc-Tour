#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfHoleActor.generated.h"

class ABasketActor;
class ADiscGolfFlyoverRouteActor;

UCLASS()
class DISCGOLFTOUR_API ADiscGolfHoleActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfHoleActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole") int32 HoleNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole") int32 Par = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole") FText HoleName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole") FVector TeeLocation = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole") FVector BasketLocation = FVector(11000, 800, 0);
    UPROPERTY(BlueprintReadOnly, Category="Hole") TObjectPtr<ABasketActor> Basket;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Course") FName CourseId = TEXT("RegressionCourse");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Course") FName LayoutId = TEXT("Practice");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Course") int32 DefinitionVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Course") FString DefinitionSource = TEXT("SOURCE BUILT");
    UPROPERTY(BlueprintReadOnly, Category="Course") int32 SurfaceCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Course") int32 WorldFixtureCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Course") int32 LandingZoneCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Course") int32 ShotRouteCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Course") int32 CameraAnchorCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Course") int32 SpectatorBoundaryCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Course") int32 WindZoneCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Course") TObjectPtr<ADiscGolfFlyoverRouteActor> FlyoverRoute;

    UFUNCTION(BlueprintPure, Category="Hole") float GetMeasuredDistanceFeet() const;
    UFUNCTION(BlueprintPure, Category="Hole") float GetEffectiveDistanceFeet() const;
    UFUNCTION(BlueprintPure, Category="Course") bool IsAuthoredBlockout() const { return CourseId == TEXT("PineRidgeChampionship"); }
    UFUNCTION(BlueprintPure, Category="Course") FString GetCourseSummary() const;
};
