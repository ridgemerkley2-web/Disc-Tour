#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DevCourseBootstrap.generated.h"

class ADiscGolfHoleActor;
class ADiscGolfFlyoverRouteActor;
class ADiscGolfLevelDesignReviewActor;
class ADiscGolfFixturePresentationActor;
class ADiscGolfFoliagePresentationActor;
class ADiscGolfTerrainPresentationActor;
class ADiscGolfWaterPresentationActor;
class ADiscGolfWorldFixtureActor;
class ADiscGolfEnvironmentController;
class ADiscGolfEnvironmentZoneActor;
class UStaticMesh;
struct FDiscGolfCollisionFixtureDefinition;
struct FDiscGolfHoleBlockoutDefinition;
enum class ECourseSurfaceType : uint8;

UCLASS()
class DISCGOLFTOUR_API ADevCourseBootstrap : public AActor
{
    GENERATED_BODY()

public:
    ADevCourseBootstrap();
    ADiscGolfHoleActor* BuildPracticeHole();
    ADiscGolfHoleActor* BuildPineRidgeHole1(FString& OutError);
    ADiscGolfHoleActor* BuildPineRidgeHole(int32 HoleNumber, FString& OutError);
    ADiscGolfHoleActor* BuildPersistentPineRidgeCourse(
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
        FString& OutError);
    bool ActivatePineRidgeHole(int32 HoleNumber, FString& OutError);
    ADiscGolfHoleActor* BuildCourse(const FName& CourseId, FString& OutError);
    void DestroyGeneratedCourse();
    UFUNCTION(BlueprintPure) ADiscGolfHoleActor* GetHole() const { return Hole; }
    UFUNCTION(BlueprintPure) ADiscGolfFlyoverRouteActor* GetFlyoverRoute() const { return FlyoverRoute; }
    UFUNCTION(BlueprintPure) ADiscGolfLevelDesignReviewActor* GetLevelDesignReview() const { return LevelDesignReview; }
    UFUNCTION(BlueprintPure) ADiscGolfFoliagePresentationActor* GetFoliagePresentation() const { return FoliagePresentation; }
    UFUNCTION(BlueprintPure) ADiscGolfWaterPresentationActor* GetWaterPresentation() const { return WaterPresentation; }
    UFUNCTION(BlueprintPure) ADiscGolfTerrainPresentationActor* GetCourseTerrainPresentation() const { return CourseTerrainPresentation; }
    UFUNCTION(BlueprintPure) ADiscGolfEnvironmentController* GetEnvironmentController() const { return EnvironmentController; }
    UFUNCTION(BlueprintPure) int32 GetEnvironmentZoneCount() const { return EnvironmentZones.Num(); }
    UFUNCTION(BlueprintPure) FName GetBuiltCourseId() const { return BuiltCourseId; }
    UFUNCTION(BlueprintPure) int32 GetPersistentHoleCount() const { return PineRidgeHoles.Num(); }
    ADiscGolfFoliagePresentationActor* GetFoliagePresentationForHole(int32 HoleNumber) const;

private:
    UPROPERTY() TObjectPtr<ADiscGolfHoleActor> Hole;
    UPROPERTY() TObjectPtr<ADiscGolfFlyoverRouteActor> FlyoverRoute;
    UPROPERTY() TObjectPtr<ADiscGolfLevelDesignReviewActor> LevelDesignReview;
    UPROPERTY() TObjectPtr<ADiscGolfFoliagePresentationActor> FoliagePresentation;
    UPROPERTY() TObjectPtr<ADiscGolfWaterPresentationActor> WaterPresentation;
    UPROPERTY() TObjectPtr<ADiscGolfTerrainPresentationActor> CourseTerrainPresentation;
    UPROPERTY() TObjectPtr<ADiscGolfEnvironmentController> EnvironmentController;
    UPROPERTY() TArray<TObjectPtr<ADiscGolfEnvironmentZoneActor>> EnvironmentZones;
    UPROPERTY() TArray<TObjectPtr<AActor>> SpawnedCourseActors;
    UPROPERTY() TMap<int32, TObjectPtr<ADiscGolfHoleActor>> PineRidgeHoles;
    UPROPERTY() TMap<int32, TObjectPtr<ADiscGolfFlyoverRouteActor>> PineRidgeFlyovers;
    UPROPERTY() TMap<int32, TObjectPtr<ADiscGolfLevelDesignReviewActor>> PineRidgeReviews;
    UPROPERTY() TMap<int32, TObjectPtr<ADiscGolfFoliagePresentationActor>> PineRidgeFoliage;
    UPROPERTY() TMap<int32, TObjectPtr<ADiscGolfWaterPresentationActor>> PineRidgeWater;
    FName BuiltCourseId = NAME_None;
    bool bLightingSpawned = false;

    AActor* SpawnPrimitive(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
        const FRotator& Rotation = FRotator::ZeroRotator);
    AActor* SpawnCourseSurface(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
        ECourseSurfaceType SurfaceType, const FRotator& Rotation = FRotator::ZeroRotator);
    void TrackActor(AActor* Actor);
    void SpawnTree(FName FixtureId, const FVector& Location, float HeightScale = 1.0f,
        bool bHideCollisionProxy = false);
    ADiscGolfWorldFixtureActor* SpawnCollisionFixture(
        const FDiscGolfCollisionFixtureDefinition& Definition, UStaticMesh* Mesh);
    ADiscGolfFixturePresentationActor* SpawnFixturePresentation(
        const FDiscGolfCollisionFixtureDefinition& Definition,
        float GrassDensityScale,
        float CullDistanceScale);
    void SpawnLighting();
    bool BuildPineRidgeHole1Environment(
        const FDiscGolfHoleBlockoutDefinition& Definition,
        FString& OutError);
    bool WritePineRidgeHole1EnvironmentStatistics(
        const FDiscGolfHoleBlockoutDefinition& Definition,
        FString& OutError) const;
    ADiscGolfHoleActor* BuildAuthoredHole(
        const FDiscGolfHoleBlockoutDefinition& Definition,
        const FString& Source,
        FString& OutError,
        bool bResetCourse = true);
};
