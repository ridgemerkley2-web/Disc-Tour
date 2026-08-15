#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfFixturePresentationActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FDiscGolfCollisionFixtureDefinition;

/** Collision-free production visuals fitted around an unchanged authored fixture proxy. */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfFixturePresentationActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfFixturePresentationActor();

    bool Configure(
        const FDiscGolfCollisionFixtureDefinition& Definition,
        float GrassDensityScale,
        float CullDistanceScale);

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    bool IsCollisionInvariant() const;

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetBrushInstanceCount() const;

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    EDiscGolfFixtureType GetFixtureType() const { return FixtureType; }

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BoulderMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BrushInstances;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SignBoard;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SignPostA;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> SignPostB;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> SignText;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> SignTextBack;

    UPROPERTY(VisibleAnywhere, Category="Course|Presentation") FName FixtureId = NAME_None;
    UPROPERTY(VisibleAnywhere, Category="Course|Presentation")
    EDiscGolfFixtureType FixtureType = EDiscGolfFixtureType::Unknown;

    void HideAllVisuals();
    bool ConfigureBoulder(const FDiscGolfCollisionFixtureDefinition& Definition);
    bool ConfigureBrush(
        const FDiscGolfCollisionFixtureDefinition& Definition,
        float GrassDensityScale,
        float CullDistanceScale);
    bool ConfigureSign(const FDiscGolfCollisionFixtureDefinition& Definition);
};
