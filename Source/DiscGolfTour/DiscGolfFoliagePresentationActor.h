#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfFoliagePresentationActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
struct FDiscGolfHoleBlockoutDefinition;
struct FDiscGolfHoleVisualPlan;

namespace DiscGolfFoliagePresentation
{
    DISCGOLFTOUR_API int32 ResolveDecorativeInstanceTarget(
        const FDiscGolfHoleVisualPlan& Plan,
        float QualityDensityScale);
}

/**
 * Deterministic, non-colliding visual understory for an authored hole.
 * Competitive tree collision remains in the authored trunk proxies.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfFoliagePresentationActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfFoliagePresentationActor();

    bool Configure(
        const FDiscGolfHoleBlockoutDefinition& HoleDefinition,
        const FDiscGolfHoleVisualPlan& Plan,
        float QualityDensityScale,
        float QualityCullDistanceScale);

    void AddAuthoredTreeVisual(const FVector& Location, float HeightScale, int32 VariantIndex);

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetVisualInstanceCount() const;

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetDecorativeInstanceCount() const { return DecorativeInstanceCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetDecorativeInstanceTarget() const { return DecorativeInstanceTarget; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    FName GetForestReferenceId() const { return ForestReferenceId; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    bool IsReady() const { return bReady; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    bool IsCollisionInvariant() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SaplingA;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SaplingB;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SaplingC;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShadowSaplingA;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShadowSaplingB;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShadowSaplingC;

    int32 DecorativeInstanceCount = 0;
    int32 DecorativeInstanceTarget = 0;
    FName ForestReferenceId = NAME_None;
    bool bReady = false;
};
