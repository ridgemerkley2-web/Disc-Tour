#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfAppearanceComponent.generated.h"

class USkeletalMeshComponent;

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfAppearanceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable)
    void ApplyStandardMorphs(USkeletalMeshComponent* Mesh, const FDGBodyProfile& Body) const;
};
