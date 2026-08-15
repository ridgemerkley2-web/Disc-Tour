#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DGFrameworkEnvironmentTypes.h"
#include "DiscGolfEnvironmentBridgeComponent.generated.h"

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfEnvironmentBridgeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfEnvironmentBridgeComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Environment")
    FDGCourseEnvironmentState CurrentState;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Environment")
    void ApplyEnvironmentState(const FDGCourseEnvironmentState& NewState);

    // Implement in BP using Ultra Dynamic Sky if acquired, otherwise built-in UE.
    UFUNCTION(BlueprintImplementableEvent, Category="Disc Golf|Environment")
    void ApplyEnvironmentToProvider(const FDGCourseEnvironmentState& State);
};
