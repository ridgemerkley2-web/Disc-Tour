#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfWaterPresentationActor.generated.h"

class UProceduralMeshComponent;
struct FDiscGolfBlockoutSurfaceDefinition;

/**
 * Presentation-only lake surface fitted to an authored cylindrical hazard.
 * The hidden course-surface primitive remains the sole collision, lie, and penalty authority.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfWaterPresentationActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfWaterPresentationActor();

    bool Configure(
        const FDiscGolfBlockoutSurfaceDefinition& Surface,
        const FString& WaterMaterialPath);

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    bool IsReady() const { return bReady; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetTriangleCount() const { return TriangleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    bool IsCollisionInvariant() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> WaterMesh;

    UPROPERTY(VisibleAnywhere)
    int32 TriangleCount = 0;

    UPROPERTY(VisibleAnywhere)
    bool bReady = false;
};
