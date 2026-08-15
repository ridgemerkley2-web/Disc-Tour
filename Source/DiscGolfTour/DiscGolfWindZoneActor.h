#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfWindZoneActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Authored deterministic local wind modifier. */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfWindZoneActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfWindZoneActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind Zone") FName ZoneId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind Zone") float BaseWindScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind Zone") FVector AdditiveWindMps = FVector::ZeroVector;

    void Configure(const FDiscGolfWindZoneDefinition& Definition, UStaticMesh* VisualizationMesh);
    bool ContainsPoint(const FVector& WorldLocationCm) const;
    FVector ModifyWind(const FVector& WindMps) const;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> ZoneBounds;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visualization;
};
