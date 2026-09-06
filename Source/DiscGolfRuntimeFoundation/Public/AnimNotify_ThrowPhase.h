#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DiscGolfCharacterTypes.h"
#include "AnimNotify_ThrowPhase.generated.h"

UCLASS()
class DISCGOLFRUNTIMEFOUNDATION_API UAnimNotify_ThrowPhase : public UAnimNotify
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGThrowPhase Phase = EDGThrowPhase::Idle;
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
