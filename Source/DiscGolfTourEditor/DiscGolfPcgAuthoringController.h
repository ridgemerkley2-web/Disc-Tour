#pragma once

#include "CoreMinimal.h"
#include "DiscGolfEnvironmentController.h"
#include "DiscGolfPcgAuthoringController.generated.h"

class UPCGComponent;

/** Editor-only graph owner. No part of this class is compiled into a game target. */
UCLASS(BlueprintType, ClassGroup=(Procedural))
class DISCGOLFTOUREDITOR_API ADiscGolfPcgAuthoringController final
    : public ADiscGolfEnvironmentController
{
    GENERATED_BODY()

public:
    ADiscGolfPcgAuthoringController();
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="PCG Authoring")
    TObjectPtr<UPCGComponent> PCGComponent;

    UFUNCTION(BlueprintCallable, CallInEditor, Category="PCG Authoring")
    void ApplyAuthoringPreset();

    UFUNCTION(BlueprintCallable, CallInEditor, Category="PCG Authoring")
    void GenerateForest();

    UFUNCTION(BlueprintCallable, CallInEditor, Category="PCG Authoring")
    void CleanupForest();
};
