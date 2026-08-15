#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "DiscGolfTypes.h"
#include "DiscGolfCourseSurfaceActor.generated.h"

/** Typed course collision surface used by authored maps and the primitive regression hole. */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfCourseSurfaceActor : public AStaticMeshActor
{
    GENERATED_BODY()

public:
    ADiscGolfCourseSurfaceActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course Surface")
    ECourseSurfaceType SurfaceType = ECourseSurfaceType::Fairway;

    UFUNCTION(BlueprintCallable, Category="Course Surface")
    void SetCourseSurfaceType(ECourseSurfaceType InSurfaceType);

    UFUNCTION(BlueprintPure, Category="Course Surface")
    ECourseSurfaceType GetCourseSurfaceType() const { return SurfaceType; }
};
